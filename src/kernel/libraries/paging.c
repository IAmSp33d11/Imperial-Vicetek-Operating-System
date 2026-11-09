// paging.c
// Handles mapping pages, copying Page Maps, etc.

#include <stdint.h> // Included for uint8_t and the others
#include <stdbool.h>  // Included for true/false
#include <serial.h> // Included for debug printing
#include <shit.h> // Included for hcf and shit
#include <mem.h> // Included for allocating a 4KiB page for the page map.
#include <paging.h> // Included cause that's what this is for.

extern uint64_t _kernel_end; // defined in linker.lds

// Sized Flags
// 15:14 Reserved (0)
// 13 - XD
// 12:9 - PK
// 8 - G
// 7 - PAT
// 6 - D
// 5 - A
// 4 - PCD
// 3 - PWT
// 2 - U/S
// 1 - R/W
// 0 - P
// AVL 0:9 used
void map_page(uint64_t* page_map, uint64_t virt_addr, uint64_t phys_addr, uint64_t hhdm, uint8_t phys_addr_size, uint16_t avl, bool overwrite, uint16_t sized_flags, bool mib_2) {
	uint64_t mask = ((1ULL << phys_addr_size) - 1) & ~4095ULL;

	if (phys_addr & mask != phys_addr) { // Make sure its not out of range and the CPU actually supports it.
		printf("Kernel Panic: Attempted to map out of range physical address 0x%uxl to virtual address 0x%uxl!\nCPU supports %udb bits!\nMask used to check: 0x%uxl\n", phys_addr, virt_addr, phys_addr_size, mask);
		hcf();
	}

	// Canonical check
	{ // Temporary stuff
		uint64_t a = virt_addr >> 47; // Check it using 4 level, since I couldn't give a shit about PML5
		if (a != 0x1FFFF && a != 0) { // Its not canonical.
			printf("Kernel Panic: Attempted to map non-canonical virtual address 0x%uxl to physical address 0x%uxl!\n", virt_addr, phys_addr);
			hcf();
		}
	}

	// Okay we can say that its not an illegal mapping that it seems.
	// I will add more checks later probably that make sure its not illegal in other ways.

	// 39 because 48 - 9, and each page map level has 0x1FF entries, aka 512 - 1 since it's 0 indexed.
	uint64_t idx = (virt_addr >> 39) & 0x1FF;

	uint64_t* pdpte;
	if (overwrite || !(page_map[idx] & 1)) { // If the present bit is set, or we want to overwrite, then
		// Allocate the pdpte
		if (!(page_map[idx] & 1)) {
			pdpte = alloc_page();
			if (pdpte == NULL) {
				printf("We fucked up.\nPDPTE\n");
				hcf();
			}
			memset(pdpte, 0, 0x1000);
		} else {
			pdpte = (uint64_t*) ((page_map[idx] & mask) + hhdm);
		}

		uint64_t pdpte_phys = (uint64_t)pdpte - hhdm;
		uint64_t temp = 0;
		temp |= pdpte_phys; // Set the phys address of pdpte
		temp |= (0) << 63; // Set XD
		temp |= 0b111; // Set Present, RW, and User to make it the most permissive.
		page_map[idx] = temp;
	} else { // Otherwise, we just grab the existing one
		pdpte = (uint64_t*) ((page_map[idx] & mask) + hhdm);
	}

	idx = (virt_addr >> 30) & 0x1FF;

	uint64_t* pde;
	if (overwrite || !(pdpte[idx] & 1)) { // If the present bit is set, or we want to overwrite, then
		// Allocate the pde
		if (!(pdpte[idx] & 1)) {
			pde = alloc_page();
			if (pde == NULL) {
				printf("We fucked up.\nPDEn\n");
				hcf();
			}
			memset(pde, 0, 0x1000);
		} else {
			pde = (uint64_t*) ((pdpte[idx] & mask) + hhdm);
			
		}

		uint64_t pde_phys = (uint64_t)pde - hhdm;
		uint64_t temp = 0;
		temp |= pde_phys; // Set the phys address of pdpte
		temp |= (0) << 63; // Set XD
		temp |= 0b111; // Set Present, RW, and User to make it the most permissive.
		pdpte[idx] = temp;
	} else { // Otherwise, we just grab the existing one
		pde = (uint64_t*) ((pdpte[idx] & mask) + hhdm);
	}


	if (mib_2) {
		if (overwrite || !(pde[idx] & 1)) {
			uint64_t temp = 0;
			temp |= phys_addr; // Set the physical address of the page.
			temp |= ((flags >> 13) & 1ULL) << 63; // Set XD
			temp |= (flags & 0b1111111); // Set the lower 7 flags
			temp |= 1 << 8; // Tell the CPU this is a 2MiB page.
			temp |= ((flags >> 8) & 1ULL) << 8; // Set the Global Bit
			temp |= ((flags >> 9) & 3ULL) << 59; // Set the Protection Key or whatever
			temp |= ((flags >> 7) & 1ULL) << 12; // Set the PAT Bit
		}
		return;
	}

	

	return;
}

// Returns the physical address of said page map.
uint64_t make_page_map() {
	void* page_map = alloc_page(); // Allocate the PML4
	memset(page_map, 0, 0x1000); // Zero it out
}