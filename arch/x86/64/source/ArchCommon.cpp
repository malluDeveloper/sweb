/**
 * @file ArchCommon.cpp
 *
 */

#include "ArchCommon.h"
#include "multiboot.h"
#include "debug_bochs.h"
#include "offsets.h"
#include "kprintf.h"
#include "kstring.h"
#include "ArchMemory.h"
#include "FrameBufferConsole.h"
#include "TextConsole.h"

extern void* kernel_end_address;

multiboot_info_t* multi_boot_structure_pointer = (multiboot_info_t*)0xDEADDEAD; // must not be in bss segment
static struct multiboot_remainder mbr __attribute__ ((section (".data"))); // must not be in bss segment

extern "C" void parseMultibootHeader()
{
  uint32 i;
  multiboot_info_t *mb_infos = *(multiboot_info_t**)VIRTUAL_TO_PHYSICAL_BOOT( (pointer)&multi_boot_structure_pointer);
  struct multiboot_remainder &orig_mbr = (struct multiboot_remainder &)(*((struct multiboot_remainder*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)&mbr)));

  if (mb_infos && mb_infos->flags & 1<<11)
  {
    struct vbe_mode* mode_info = (struct vbe_mode*)(uint64)mb_infos->vbe_mode_info;
    orig_mbr.have_vesa_console = 1;
    orig_mbr.vesa_lfb_pointer = mode_info->phys_base;
    orig_mbr.vesa_x_res = mode_info->x_resolution;
    orig_mbr.vesa_y_res = mode_info->y_resolution;
    orig_mbr.vesa_bits_per_pixel = mode_info->bits_per_pixel;
  }

  if (mb_infos && mb_infos->flags && 1<<3)
  {
    module_t * mods = (module_t*)(uint64)mb_infos->mods_addr;
    for (i=0;i<mb_infos->mods_count;++i)
    {
      orig_mbr.module_maps[i].used = 1;
      orig_mbr.module_maps[i].start_address = mods[i].mod_start;
      orig_mbr.module_maps[i].end_address = mods[i].mod_end;
      strncpy((char*)(uint64)orig_mbr.module_maps[i].name, (const char*)(uint64)mods[i].string, 256);
    }
    orig_mbr.num_module_maps = mb_infos->mods_count;
  }

  for (i=0;i<MAX_MEMORY_MAPS;++i)
  {
    orig_mbr.memory_maps[i].used = 0;
  }

  if (mb_infos && mb_infos->flags & 1<<6)
  {
    uint32 mmap_size = sizeof(memory_map);
    uint32 mmap_total_size = mb_infos->mmap_length;
    uint32 num_maps = mmap_total_size / mmap_size;

    for (i=0;i<num_maps;++i)
    {
      memory_map * map = (memory_map*)(uint64)(mb_infos->mmap_addr+mmap_size*i);
      orig_mbr.memory_maps[i].used = 1;
      orig_mbr.memory_maps[i].start_address = map->base_addr_low;
      orig_mbr.memory_maps[i].end_address = map->base_addr_low + map->length_low;
      orig_mbr.memory_maps[i].type = map->type;
    }
  }
}

pointer ArchCommon::getKernelEndAddress()
{
   return (pointer)&kernel_end_address;
}

pointer ArchCommon::getFreeKernelMemoryStart()
{
   return (pointer)&kernel_end_address;
}

pointer ArchCommon::getFreeKernelMemoryEnd()
{
   return 0xFFFFFFFF80400000ULL;
}


uint32 ArchCommon::haveVESAConsole(uint32 is_paging_set_up)
{
  if (is_paging_set_up)
    return mbr.have_vesa_console;
  else
  {
    struct multiboot_remainder &orig_mbr = (struct multiboot_remainder &)(*((struct multiboot_remainder*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)&mbr)));
    return orig_mbr.have_vesa_console;
  }
}

uint32 ArchCommon::getNumModules(uint32 is_paging_set_up)
{
  if (is_paging_set_up)
    return mbr.num_module_maps;
  else
  {
    struct multiboot_remainder &orig_mbr = (struct multiboot_remainder &)(*((struct multiboot_remainder*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)&mbr)));
    return orig_mbr.num_module_maps;
  }
}

uint32 ArchCommon::getModuleStartAddress(uint32 num,uint32 is_paging_set_up)
{
  if (is_paging_set_up)
    return mbr.module_maps[num].start_address | PHYSICAL_TO_VIRTUAL_OFFSET;
  else
  {
    struct multiboot_remainder &orig_mbr = (struct multiboot_remainder &)(*((struct multiboot_remainder*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)&mbr)));
    return orig_mbr.module_maps[num].start_address | PHYSICAL_TO_VIRTUAL_OFFSET;
  }
}

uint32 ArchCommon::getModuleEndAddress(uint32 num,uint32 is_paging_set_up)
{
  if (is_paging_set_up)
    return mbr.module_maps[num].end_address | PHYSICAL_TO_VIRTUAL_OFFSET;
  else
  {
    struct multiboot_remainder &orig_mbr = (struct multiboot_remainder &)(*((struct multiboot_remainder*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)&mbr)));
    return orig_mbr.module_maps[num].end_address | PHYSICAL_TO_VIRTUAL_OFFSET;
  }
}

uint32 ArchCommon::getVESAConsoleHeight()
{
  return mbr.vesa_y_res;
}

uint32 ArchCommon::getVESAConsoleWidth()
{
  return mbr.vesa_x_res;
}

pointer ArchCommon::getVESAConsoleLFBPtr(uint32 is_paging_set_up)
{
  if (is_paging_set_up)
    return 0xFFFFFFFFC000000ULL - 1024ULL * 1024ULL * 16ULL;
  else
  {
    struct multiboot_remainder &orig_mbr = (struct multiboot_remainder &)(*((struct multiboot_remainder*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)&mbr)));
    return orig_mbr.vesa_lfb_pointer;
  }
}

pointer ArchCommon::getFBPtr(uint32 is_paging_set_up)
{
  if (is_paging_set_up)
    return PHYSICAL_TO_VIRTUAL_OFFSET | 0xB8000ULL;
  else
    return 0xB8000ULL;
}

uint32 ArchCommon::getVESAConsoleBitsPerPixel()
{
  return mbr.vesa_bits_per_pixel;
}

uint32 ArchCommon::getNumUseableMemoryRegions()
{
  uint32 i;
  for (i=0;i<MAX_MEMORY_MAPS;++i)
  {
    if (!mbr.memory_maps[i].used)
      break;
  }
  return i;
}

uint32 ArchCommon::getUsableMemoryRegion(size_t region, pointer &start_address, pointer &end_address, size_t &type)
{
  if (region >= MAX_MEMORY_MAPS)
    return 1;

  start_address = mbr.memory_maps[region].start_address;
  end_address = mbr.memory_maps[region].end_address;
  type = mbr.memory_maps[region].type;

  return 0;
}

Console* ArchCommon::createConsole(uint32 count)
{
  if (haveVESAConsole())
    return new FrameBufferConsole(count);
  else
    return new TextConsole(count);
}


#if (A_BOOT == A_BOOT | OUTPUT_ENABLED)
#define PRINT(X) writeLine2Bochs((const char*)VIRTUAL_TO_PHYSICAL_BOOT(X))
#else
#define PRINT(X)
#endif

extern SegmentDescriptor gdt[7];
extern "C" void startup();
extern "C" void initialisePaging();
extern uint8 boot_stack[0x4000];

struct GDTPtr
{
    uint16 limit;
    uint64 addr;
}__attribute__((__packed__)) gdt_ptr;

extern "C" void entry64()
{
  PRINT("Parsing Multiboot Header...\n");
  parseMultibootHeader();
  PRINT("Initializing Kernel Paging Structures...\n");
  initialisePaging();
  PRINT("Setting CR3 Register...\n");
  asm("mov %%rax, %%cr3" : : "a"(VIRTUAL_TO_PHYSICAL_BOOT(ArchMemory::getRootOfKernelPagingStructure())));
  PRINT("Switch to our own stack...\n");
  asm("mov %[stack], %%rsp\n"
      "mov %[stack], %%rbp\n" : : [stack]"i"(boot_stack + 0x4000));
  PRINT("Loading Long Mode Segments...\n");

  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.addr = (uint64)gdt;
  asm("lgdt (%%rax)" : : "a"(&gdt_ptr));
  asm("mov %%ax, %%ds\n"
      "mov %%ax, %%es\n"
      "mov %%ax, %%ss\n"
      "mov %%ax, %%fs\n"
      "mov %%ax, %%gs\n"
      : : "a"(KERNEL_DS));
  asm("ltr %%ax" : : "a"(KERNEL_TSS));
  PRINT("Calling startup()...\n");
  asm("jmp *%[startup]" : : [startup]"r"(startup));
  while (1);
}

class Stabs2DebugInfo;
Stabs2DebugInfo const *kernel_debug_info = 0;

void ArchCommon::initDebug()
{
}

void ArchCommon::idle()
{
  asm volatile("hlt");
}

void ArchCommon::drawHeartBeat()
{
  const char* clock = "/-\\|";
  static uint32 heart_beat_value = 0;
  char* fb = (char*)getFBPtr();
  fb[0] = clock[heart_beat_value++ % 4];
  fb[1] = 0x9f;
}
