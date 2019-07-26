#ifndef __ADAMANT_ELF
#define __ADAMANT_ELF

namespace adamant
{

ADM_VISIBILITY
void adm_elf_load(const char* lib) noexcept;

ADM_VISIBILITY
void adm_elf_unload(const char* lib) noexcept;

ADM_VISIBILITY
void adm_elf_init() noexcept;

ADM_VISIBILITY
void adm_elf_fini() noexcept;

}

#endif
