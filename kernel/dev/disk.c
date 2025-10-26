#include "config.h"
#include "util/types.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "util/string.h"
#include "mem/alloc.h"
#include "task/sche.h"
#include "fs/bio.h"

// virtio mmio control registers, mapped starting at 0x10001000.
// from qemu virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE      0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION          0x004 // version; should be 2
#define VIRTIO_MMIO_DEVICE_ID        0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID        0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES  0x010
#define VIRTIO_MMIO_DRIVER_FEATURES  0x020
#define VIRTIO_MMIO_QUEUE_SEL        0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX    0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM        0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_READY      0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY     0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK    0x064 // write-only
#define VIRTIO_MMIO_STATUS           0x070 // read/write
#define VIRTIO_MMIO_QUEUE_DESC_LOW   0x080 // physical address for descriptor table, write-only
#define VIRTIO_MMIO_QUEUE_DESC_HIGH  0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW  0x090 // physical address for available ring, write-only
#define VIRTIO_MMIO_DRIVER_DESC_HIGH 0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW  0x0a0 // physical address for used ring, write-only
#define VIRTIO_MMIO_DEVICE_DESC_HIGH 0x0a4

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER      2
#define VIRTIO_CONFIG_S_DRIVER_OK   4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// device feature bits
#define VIRTIO_BLK_F_RO             5  /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI           7  /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12 /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

// a single descriptor, from the spec.
struct virtq_desc {
  u64 addr;
  u32 len;
  u16 flags;
  u16 next;
};
#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

// the (entire) avail ring, from the spec.
struct virtq_avail {
  u16 flags;     // always zero
  u16 idx;       // driver will write ring[idx] next
  u16 ring[NUM]; // descriptor numbers of chain heads
  u16 unused;
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
struct virtq_used_elem {
  u32 id; // index of start of completed descriptor chain
  u32 len;
};

struct virtq_used {
  u16 flags; // always zero
  u16 idx;   // device increments when it adds a ring[] entry
  struct virtq_used_elem ring[NUM];
};

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
  u32 type; // VIRTIO_BLK_T_IN or ..._OUT
  u32 reserved;
  u64 sector;
};


//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#define VIRTIO0 0x10001000UL
// the address of virtio mmio register r.
#define R(r) ((volatile u32*)(VIRTIO0 + (r)))

static struct disk {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc* desc;

  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail* avail;

  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used* used;

  // our own book-keeping.
  char free[NUM]; // is a descriptor free?
  u16 used_idx;   // we've looked this far in used[2..NUM].

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    struct iobuf* b;
    char status;
  } info[NUM];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_blk_req ops[NUM];

  struct spinlock vdisk_lock;

} disk = { .vdisk_lock.lname = "virtio_disk" };




void
virtio_disk_init(void)
{
  u32 status = 0;

  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 || *R(VIRTIO_MMIO_VERSION) != 2 || *R(VIRTIO_MMIO_DEVICE_ID) != 2
      || *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("could not find virtio disk");
  }

  // reset device
  *R(VIRTIO_MMIO_STATUS) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // negotiate features
  u64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // ensure queue 0 is not in use.
  if (*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  // check maximum queue size.
  u32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0)
    panic("virtio disk has no queue 0");
  if (max < NUM)
    panic("virtio disk max queue too short");

  // allocate and zero queue memory.

  //! modified
  disk.desc = (struct virtq_desc*)pha(alloc_page());
  disk.avail = (struct virtq_avail*)pha(alloc_page());
  disk.used = (struct virtq_used*)pha(alloc_page());
  //


  if (!disk.desc || !disk.avail || !disk.used)
    panic("virtio disk alloc_page");

  //! modified
  memset1(disk.desc, 0, PGSIZE);
  memset1(disk.avail, 0, PGSIZE);
  memset1(disk.used, 0, PGSIZE);
  //!

  // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (u64)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (u64)disk.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (u64)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (u64)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (u64)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (u64)disk.used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for (int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc(void)
{
  for (int i = 0; i < NUM; i++) {
    if (disk.free[i]) {
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
  if (i >= NUM)
    panic("free_desc 1");
  if (disk.free[i])
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);
}

// free a chain of descriptors.
static void
free_chain(int i)
{
  while (1) {
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if (flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc3_desc(int* idx)
{
  for (int i = 0; i < 3; i++) {
    idx[i] = alloc_desc();
    if (idx[i] < 0) {
      for (int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void
virtio_disk_rw(struct iobuf* b, int write)
{
  u64 sector = b->blockno * (BSIZE / 512);

  acquire_spin(&disk.vdisk_lock);

  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];
  while (1) {
    if (alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&disk.free[0], &disk.vdisk_lock);
  }

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_req* buf0 = &disk.ops[idx[0]];

  if (write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN; // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (u64)buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (u64)b->data;
  disk.desc[idx[1]].len = BSIZE;
  if (write)
    disk.desc[idx[1]].flags = 0; // device reads b->data
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // device writes 0 on success
  disk.desc[idx[2]].addr = (u64)&disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
  disk.desc[idx[2]].next = 0;

  // record struct iobuf for virtio_disk_intr().
  b->disk = 1;
  disk.info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  disk.avail->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
  while (b->disk == 1) {
    sleep(b, &disk.vdisk_lock);
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);

  release_spin(&disk.vdisk_lock);
}

void
virtio_disk_intr(void)
{
  acquire_spin(&disk.vdisk_lock);
  /*
    ! 如果目标线程正在因硬盘事件执行sleep,这条语句可以防止唤醒丢失,保证sleep操作的原子性
  */

  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // the device increments disk.used->idx when it
  // adds an entry to the used ring.

  while (disk.used_idx != disk.used->idx) {
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if (disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct iobuf* b = disk.info[id].b;
    b->disk = 0; // disk is done with buf
    wakeup(b);

    disk.used_idx += 1;
  }

  release_spin(&disk.vdisk_lock);
}



void
init_disk(void)
{
  virtio_disk_init();
}

void
disk_read(struct iobuf* buf)
{
  virtio_disk_rw(buf, 0);
}

void
disk_write(struct iobuf* buf)
{
  virtio_disk_rw(buf, 1);
}

void
do_disk_irq(void)
{
  virtio_disk_intr();
}