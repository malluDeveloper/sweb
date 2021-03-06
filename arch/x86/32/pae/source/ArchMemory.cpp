/**
 * @file ArchMemory.cpp
 *
 */

#include "ArchMemory.h"
#include "kprintf.h"
#include "assert.h"
#include "offsets.h"
#include "PageManager.h"
#include "kstring.h"

PageDirPointerTableEntry kernel_page_directory_pointer_table[PAGE_DIRECTORY_POINTER_TABLE_ENTRIES] __attribute__((aligned(0x20)));
PageDirEntry kernel_page_directory[4 * PAGE_DIRECTORY_ENTRIES] __attribute__((aligned(0x1000)));
PageTableEntry kernel_page_tables[8 * PAGE_TABLE_ENTRIES] __attribute__((aligned(0x1000)));

ArchMemory::ArchMemory() : page_dir_pointer_table_((PageDirPointerTableEntry*) (((uint32) page_dir_pointer_table_space_ + 0x20) & (~0x1F)))
{
  memcpy(page_dir_pointer_table_, kernel_page_directory_pointer_table, sizeof(PageDirPointerTableEntry) * PAGE_DIRECTORY_POINTER_TABLE_ENTRIES);
  memset(page_dir_pointer_table_, 0, sizeof(PageDirPointerTableEntry) * PAGE_DIRECTORY_POINTER_TABLE_ENTRIES/2); // should be zero, this is just for safety
}

void ArchMemory::checkAndRemovePT(uint32 physical_page_directory_page, uint32 pde_vpn)
{
  PageDirEntry *page_directory = (PageDirEntry *) getIdentAddressOfPPN(physical_page_directory_page);
  PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
  assert(page_directory[pde_vpn].page.size == 0);

  if (!page_directory[pde_vpn].pt.present) return; // PT not present -> do nothing.

  for (uint32 pte_vpn=0; pte_vpn < PAGE_TABLE_ENTRIES; ++pte_vpn)
    if (pte_base[pte_vpn].present > 0)
      return; //not empty -> do nothing

  //else:
  page_directory[pde_vpn].pt.present = 0;
  PageManager::instance()->freePPN(page_directory[pde_vpn].pt.page_table_ppn);
}

void ArchMemory::unmapPage(uint32 virtual_page)
{
  RESOLVEMAPPING(page_dir_pointer_table_,virtual_page);

  if (page_dir_pointer_table_[pdpte_vpn].present)
  {
    if (page_directory[pde_vpn].page.size)
    {
      page_directory[pde_vpn].page.present = 0;
      //PageManager manages Pages of size PAGE_SIZE only, so we have to free this_page_size/PAGE_SIZE Pages
      for (uint32 p=0;p<PAGE_TABLE_ENTRIES;++p)
        PageManager::instance()->freePPN(page_directory[pde_vpn].page.page_ppn*1024 + p);
    }
    else
    {
      PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
      if (pte_base[pte_vpn].present)
      {
        pte_base[pte_vpn].present = 0;
        PageManager::instance()->freePPN(pte_base[pte_vpn].page_ppn);
      }
      checkAndRemovePT(page_dir_pointer_table_[pdpte_vpn].page_directory_ppn, pde_vpn);
    }
  }
}

void ArchMemory::insertPD(uint32 pdpt_vpn, uint32 physical_page_directory_page)
{
  kprintfd("insertPD: pdpt %x pdpt_vpn %x physical_page_table_page %x\n",page_dir_pointer_table_,pdpt_vpn,physical_page_directory_page);
  memset((void*)getIdentAddressOfPPN(physical_page_directory_page), 0,PAGE_SIZE);
  memset((void*)(page_dir_pointer_table_ + pdpt_vpn), 0, sizeof(PageDirPointerTableEntry));
  page_dir_pointer_table_[pdpt_vpn].page_directory_ppn = physical_page_directory_page;
  page_dir_pointer_table_[pdpt_vpn].present = 1;
}

void ArchMemory::insertPT(PageDirEntry* page_directory, uint32 pde_vpn, uint32 physical_page_table_page)
{
  kprintfd("insertPT: page_directory %x pde_vpn %x physical_page_table_page %x\n",page_directory,pde_vpn,physical_page_table_page);
  memset((void*)getIdentAddressOfPPN(physical_page_table_page), 0, PAGE_SIZE);
  memset((void*)(page_directory + pde_vpn), 0, sizeof(PageDirPointerTableEntry));
  page_directory[pde_vpn].pt.writeable = 1;
  page_directory[pde_vpn].pt.size = 0;
  page_directory[pde_vpn].pt.page_table_ppn = physical_page_table_page;
  page_directory[pde_vpn].pt.user_access = 1;
  page_directory[pde_vpn].pt.present = 1;
}

void ArchMemory::mapPage(uint32 virtual_page,
    uint32 physical_page, uint32 user_access, uint32 page_size)
{
  RESOLVEMAPPING(page_dir_pointer_table_,virtual_page);

  if (page_dir_pointer_table_[pdpte_vpn].present == 0)
  {
    uint32 ppn = PageManager::instance()->allocPPN();
    page_directory = (PageDirEntry*) getIdentAddressOfPPN(ppn);
    insertPD(pdpte_vpn, ppn);
  }

  if (page_size==PAGE_SIZE)
  {
    if (page_directory[pde_vpn].pt.present == 0)
      insertPT(page_directory,pde_vpn,PageManager::instance()->allocPPN());

    PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
    pte_base[pte_vpn].writeable = 1;
    pte_base[pte_vpn].user_access = user_access;
    pte_base[pte_vpn].page_ppn = physical_page;
    pte_base[pte_vpn].present = 1;
  }
  else if ((page_size==PAGE_SIZE*PAGE_TABLE_ENTRIES) && (page_directory[pde_vpn].page.present == 0))
  {
    page_directory[pde_vpn].page.writeable = 1;
    page_directory[pde_vpn].page.size = 1;
    page_directory[pde_vpn].page.page_ppn = physical_page;
    page_directory[pde_vpn].page.user_access = user_access;
    page_directory[pde_vpn].page.present = 1;
  }
  else
    assert(false);
}

ArchMemory::~ArchMemory()
{
  debug ( A_MEMORY,"ArchMemory::~ArchMemory(): Freeing page dir pointer table %x\n",page_dir_pointer_table_ );
  freePageDirectory(page_dir_pointer_table_[0].page_directory_ppn); // 0-1 GiB
  freePageDirectory(page_dir_pointer_table_[1].page_directory_ppn); // 1-2 GiB
}

void ArchMemory::freePageDirectory(uint32 physical_page_directory_page)
{
  PageDirEntry *page_directory = (PageDirEntry *) getIdentAddressOfPPN(physical_page_directory_page);
  for (uint32 pde_vpn=0; pde_vpn < PAGE_DIRECTORY_ENTRIES; ++pde_vpn)
  {
    if (page_directory[pde_vpn].pt.present)
    {
      if (page_directory[pde_vpn].page.size)
      {
        page_directory[pde_vpn].page.present=0;
          for (uint32 p=0;p<1024;++p)
            PageManager::instance()->freePPN(page_directory[pde_vpn].page.page_ppn*1024 + p);
      }
      else
      {
        PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
        for (uint32 pte_vpn=0; pte_vpn < PAGE_TABLE_ENTRIES; ++pte_vpn)
        {
          if (pte_base[pte_vpn].present)
          {
            pte_base[pte_vpn].present = 0;
            PageManager::instance()->freePPN(pte_base[pte_vpn].page_ppn);
          }
        }
        page_directory[pde_vpn].pt.present=0;
        PageManager::instance()->freePPN(page_directory[pde_vpn].pt.page_table_ppn);
      }
    }
  }
  PageManager::instance()->freePPN(physical_page_directory_page);
}

bool ArchMemory::checkAddressValid(uint32 vaddress_to_check)
{
  RESOLVEMAPPING(page_dir_pointer_table_, vaddress_to_check / PAGE_SIZE);
  if (page_dir_pointer_table_[pdpte_vpn].present)
  {
    if (page_directory[pde_vpn].pt.present)
    {
      if (page_directory[pde_vpn].page.size)
        return true;

      PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
      if (pte_base[pte_vpn].present)
        return true;
    }
  }
  return false;
}

uint32 ArchMemory::get_PAddr_Of_VAddr_In_KernelMapping(uint32 virtual_addr)
{
  size_t physical_addr;
  uint32 page_size = get_PPN_Of_VPN_In_KernelMapping(virtual_addr / PAGE_SIZE, &physical_addr);
  return physical_addr * page_size + (virtual_addr % PAGE_SIZE);
}

uint32 ArchMemory::get_PPN_Of_VPN_In_KernelMapping(uint32 virtual_page, size_t *physical_page, uint32 *physical_pte_page)
{
  PageDirPointerTableEntry *pdpt = kernel_page_directory_pointer_table;
  RESOLVEMAPPING(pdpt, virtual_page);
  if (pdpt[pdpte_vpn].present)
  {
    if (page_directory[pde_vpn].pt.present)
    {
      if (page_directory[pde_vpn].page.size)
      {
        if (physical_page)
          *physical_page = page_directory[pde_vpn].page.page_ppn;
        return (PAGE_SIZE*PAGE_TABLE_ENTRIES);
      }
      else
      {
        if (physical_pte_page)
          *physical_pte_page = page_directory[pde_vpn].pt.page_table_ppn;
        PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
        if (pte_base[pte_vpn].present)
        {
          if (physical_page)
            *physical_page = pte_base[pte_vpn].page_ppn;
          return PAGE_SIZE;
        }
      }
    }
  }
  return 0;
}

void ArchMemory::mapKernelPage(uint32 virtual_page, uint32 physical_page)
{
  PageDirPointerTableEntry *pdpt = kernel_page_directory_pointer_table;
  RESOLVEMAPPING(pdpt, virtual_page);
  assert(pdpt[pdpte_vpn].present);
  assert(page_directory[pde_vpn].pt.present && page_directory[pde_vpn].pt.size == 0);
  PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
  assert(!pte_base[pte_vpn].present);
  pte_base[pte_vpn].writeable = 1;
  pte_base[pte_vpn].page_ppn = physical_page;
  pte_base[pte_vpn].present = 1;
}

void ArchMemory::unmapKernelPage(uint32 virtual_page)
{
  PageDirPointerTableEntry *pdpt = kernel_page_directory_pointer_table;
  RESOLVEMAPPING(pdpt, virtual_page);
  assert(pdpt[pdpte_vpn].present);
  assert(page_directory[pde_vpn].pt.present && page_directory[pde_vpn].pt.size == 0);
  PageTableEntry *pte_base = (PageTableEntry *) getIdentAddressOfPPN(page_directory[pde_vpn].pt.page_table_ppn);
  assert(pte_base[pte_vpn].present);
  pte_base[pte_vpn].present = 0;
  pte_base[pte_vpn].writeable = 0;
  PageManager::instance()->freePPN(pte_base[pte_vpn].page_ppn);
}

PageDirPointerTableEntry* ArchMemory::getRootOfPagingStructure()
{
  return page_dir_pointer_table_;
}

PageDirPointerTableEntry* ArchMemory::getRootOfKernelPagingStructure()
{
  return kernel_page_directory_pointer_table;
}

uint32 ArchMemory::getValueForCR3()
{
  // last 5 bits must be zero!
  assert(((uint32)page_dir_pointer_table_ & 0x1F) == 0);
  return get_PAddr_Of_VAddr_In_KernelMapping((uint32)page_dir_pointer_table_);
}

pointer ArchMemory::getIdentAddressOfPPN(uint32 ppn, uint32 page_size /* optional */)
{
  return (3U*1024U*1024U*1024U) + (ppn * page_size);
}
