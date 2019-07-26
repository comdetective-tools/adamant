#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <link.h>
#include <elf.h>

#include <adm_config.h>
#include <adm_memory.h>
#include <adm_hash.h>
#include <adm_database.h>
#include <adm_elf.h>

using namespace adamant;

static table_t<ADM_ELF_SYMS_MAPLOG>* syms;
static table_t<ADM_ELF_LIBS_MAPLOG>* libs;
static pool_t<char, ADM_ELF_NAMES_BLOCKSIZE>* names;
static pool_t<hash_t, ADM_ELF_HASHES_BLOCKSIZE>* hashes;

static void
adm_elf_parse_table(char* lib, const char* str, const Elf64_Sym* symbols, const uint64_t snum, const uint64_t base, const uint32_t local)
{
  uint64_t sf = 0; char* fname=nullptr;
  uint64_t s = 0;
  for(; s < local; ++s) {
    if(ELF64_ST_TYPE(symbols[s].st_info)==STT_OBJECT && symbols[s].st_size) {
      if(fname==nullptr) {
        fname = names->malloc(strlen(str+symbols[sf].st_name)+1);
        if(fname==nullptr)
          adm_warning("Cannot allocate symbol name ", str+symbols[sf].st_name, "!");
        else
          strcpy(fname, str+symbols[sf].st_name);
      }
      adm_object_t* obj = adm_db_insert(base+symbols[s].st_value, symbols[s].st_size);
      if(obj) {
        obj->meta.meta[ADM_META_VAR_TYPE] = names->malloc(strlen(str+symbols[s].st_name)+1);
        strcpy(static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]), str+symbols[s].st_name);
        obj->meta.meta[ADM_META_OBJ_TYPE] = fname;
        obj->meta.meta[ADM_META_BIN_TYPE] = lib;
      }
    }
    else if(ELF64_ST_TYPE(symbols[s].st_info)==STT_FILE) {
      sf=s; fname=nullptr;
    }
  }

  while(s<snum) {
    if(ELF64_ST_TYPE(symbols[s].st_info)==STT_OBJECT
       && symbols[s].st_size &&
        (ELF64_ST_BIND(symbols[s].st_info)==STB_GLOBAL
       ||ELF64_ST_BIND(symbols[s].st_info)==STB_WEAK)) {

      char* value = const_cast<char*>(syms->find(str+symbols[s].st_name));
      switch(ELF64_ST_VISIBILITY(symbols[s].st_other)) {
      case STV_DEFAULT:
        if(value==nullptr) {
          hash_t* entry = hashes->malloc();
          if(entry)
            value = names->malloc(strlen(str+symbols[s].st_name)+1);
          if(value) {
            strcpy(value, str+symbols[s].st_name);
            entry->value = value;
            syms->insert(entry);
          }
          else
            adm_warning("Cannot allocate symbol name ", str+symbols[s].st_name, "!");

          adm_object_t* obj = adm_db_insert(base+symbols[s].st_value, symbols[s].st_size);
          if(obj) {
            obj->meta.meta[ADM_META_VAR_TYPE] = value;
            obj->meta.meta[ADM_META_BIN_TYPE] = lib;
          }
        }
        break;
      case STV_PROTECTED:
        {
          if(value==nullptr) {
            hash_t* entry = hashes->malloc();
            if(entry)
              value = names->malloc(strlen(str+symbols[s].st_name)+1);
            if(value) {
              strcpy(value, str+symbols[s].st_name);
              entry->value = value;
              syms->insert(entry);
            }
            else
              adm_warning("Cannot allocate symbol name ", str+symbols[s].st_name, "!");
          }
          adm_object_t* obj = adm_db_insert(base+symbols[s].st_value, symbols[s].st_size);
          if(obj) {
            obj->meta.meta[ADM_META_VAR_TYPE] = value;
            obj->meta.meta[ADM_META_BIN_TYPE] = lib;
          }
        }
      default: break;
      }
    }
    ++s;
  }
}

static void
adm_elf_read_symbols(const char* filename, uint64_t base)
{
  hash_t* entry = hashes->malloc();
  char* libname = names->malloc(strlen(filename)+1);
  if(libname) {
    entry->value = strcpy(libname, filename);
    libs->insert(entry);
  }
  
  std::ifstream object(filename);
  if(!object.is_open())
    adm_warning("Unable to open object ", filename);
  else {
    Elf64_Ehdr ehdr;
    object.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

    object.seekg(ehdr.e_shoff+ehdr.e_shstrndx*ehdr.e_shentsize, std::ios_base::beg);
    Elf64_Shdr shdr;
    object.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
    //char* shstr = static_cast<char*>(malloc_ptr(shdr.sh_size));
    char* shstr = static_cast<char*>(malloc(shdr.sh_size));
    object.seekg(shdr.sh_offset, std::ios_base::beg);
    object.read(shstr, shdr.sh_size);

    char* dynstr = nullptr;
    char* strtab = nullptr;
    Elf64_Sym* dynsym = nullptr;
    Elf64_Sym* symtab = nullptr;
    uint64_t dsym = 0, tsym = 0;
    uint32_t local = 0;

    object.seekg(ehdr.e_shoff, std::ios_base::beg);
    for(uint16_t sh=0; sh<ehdr.e_shnum; ++sh) {
      object.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
      off_t off = object.tellg();
      void* ptr = nullptr;
      switch(shdr.sh_type) {
      case SHT_STRTAB:
      case SHT_SYMTAB:
      case SHT_DYNSYM:
        if(/*(ptr=malloc_ptr(shdr.sh_size))*/ (ptr=malloc(shdr.sh_size))) {
          object.seekg(shdr.sh_offset, std::ios_base::beg);
          object.read(static_cast<char*>(ptr), shdr.sh_size);
          if(strcmp(shstr+shdr.sh_name, ".strtab")==0)
            strtab = static_cast<char*>(ptr);
          else if(strcmp(shstr+shdr.sh_name, ".dynstr")==0)
            dynstr = static_cast<char*>(ptr);
          else if(strcmp(shstr+shdr.sh_name, ".symtab")==0) {
            symtab = static_cast<Elf64_Sym*>(ptr);
            tsym = shdr.sh_size/shdr.sh_entsize;
            local = shdr.sh_info;
          }
          else if(strcmp(shstr+shdr.sh_name, ".dynaminc")==0) {
            dynsym = static_cast<Elf64_Sym*>(ptr);
            dsym = shdr.sh_size/shdr.sh_entsize;
          }
        }
        else
          adm_warning("Unable to allocate buffer reading ", filename, "!");
      default: break;
      }
      object.seekg(off, std::ios_base::beg);
    }
    if(symtab && strtab)
      adm_elf_parse_table(libname, strtab, symtab, tsym, base, local);
    else if(dynstr && dynsym)
      adm_elf_parse_table(libname, dynstr, dynsym, dsym, base, local);

    if(shstr) free(shstr);//free_ptr(shstr);
    if(strtab) free(strtab);//free_ptr(strtab);
    if(dynstr) free(dynstr);//free_ptr(dynstr);
    if(symtab) free(symtab);//free_ptr(symtab);
    if(dynsym) free(dynsym);//free_ptr(dynsym);

    object.close();
  }
}

static int
adm_elf_callback(struct dl_phdr_info *info, size_t size, void *data)
{
  uint64_t pagemask = sysconf(_SC_PAGESIZE);
  pagemask = ~(pagemask-1);
  if(strcmp("",info->dlpi_name)==0) {
    ssize_t len = readlink("/proc/self/exe", static_cast<char*>(data), ADM_MAX_PATH);
    if(len>0) {
      static_cast<char*>(data)[len]='\0';
      adm_elf_read_symbols(static_cast<char*>(data), info->dlpi_addr);
    }
    else {
        printf("Unable to open program image!");
	//adm_warning("Unable to open program image!");
	}
  }
  else {
    for(Elf64_Half h = 0; h < info->dlpi_phnum; h++)
      if((info->dlpi_phdr[h].p_flags&PF_R) && (!(info->dlpi_phdr[h].p_flags&PF_X)) &&
          adm_conf_string("+bin", info->dlpi_name)) {
        adm_elf_read_symbols(info->dlpi_name, info->dlpi_addr);
        break;
      }
  }

  return 0;
}

ADM_VISIBILITY
void adamant::adm_elf_load(const char* add) noexcept
{
  /* TODO
    to do this needs main and other scopes

    scope { map, name (lib or "main"), flag?, handle}
    iterate libs not in main scope
      add to new scope
      for each symbol search other scopes
        if not found and flag is GLOBAL add to new scope
  */
}

ADM_VISIBILITY
void adamant::adm_elf_unload(const char* add) noexcept
{
  /* TODO
    to do this needs maps with pointers to objects, and maps of local symbols

    find scope for the given handle { map, name (lib or "main"), flag?, handle}
    if flag != NOCLOSE iterate over map
      for each symbol in this scope remove from database
  */
}

ADM_VISIBILITY
void adamant::adm_elf_init() noexcept
{
  //printf("begin adm_elf_init\n");
  syms = new table_t<ADM_ELF_SYMS_MAPLOG>;
  libs = new table_t<ADM_ELF_LIBS_MAPLOG>;
  names = new pool_t<char, ADM_ELF_NAMES_BLOCKSIZE>;
  hashes = new pool_t<hash_t, ADM_ELF_HASHES_BLOCKSIZE>;

  char buffer[ADM_MAX_PATH];
  std::ifstream map("/proc/self/maps");
  if(!map.is_open())
    adm_warning("Unable to open process memory map!");
  else {
    //printf("within else\n");
    uint64_t start, end, offset, inode;
    char perms[5]; char device[6];
    while(1) {
      char name[ADM_MAX_PATH];
      map.getline(buffer, ADM_MAX_PATH);
      int ret=sscanf(buffer, "%" PRIx64 "-%" PRIx64 " %4s %" PRIx64 " %5s %" PRIx64 " %s\n", &start, &end, perms, &offset, device, &inode, name);
      if(ret==7 && strcmp(name,"[stack]")==0) {
        adm_object_t* obj = adm_db_insert(start, end-start);
        if(obj) {
          obj->meta.meta[ADM_META_VAR_TYPE] = names->malloc(sizeof("[stack]"+1));
          strcpy(static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]), "[stack]");
        }
        break;
      }
    }
    map.close();
    //printf("ending else\n");
  }
  dl_iterate_phdr(adm_elf_callback, buffer);
  //printf("end adm_elf_init\n");
}

ADM_VISIBILITY
void adamant::adm_elf_fini() noexcept
{
  delete syms;
  delete libs;
  delete names;
  delete hashes;
}
