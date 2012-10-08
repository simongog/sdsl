#include "sdsl/memory_management.hpp"

#include <cstdlib> // for malloc and free
#include <sys/mman.h>

#ifdef MAP_HUGETLB
	#define HUGE_LEN 1073741824 
	#define HUGE_PROTECTION (PROT_READ | PROT_WRITE)
	#define HUGE_FLAGS (MAP_HUGETLB | MAP_ANONYMOUS | MAP_PRIVATE)
#endif

//! Namespace for the succinct data structure library
namespace sdsl
{
	static int nifty_counter;
	std::map<uint64_t, mm_item_base*> mm::m_items;
	uint64_t mm::m_total_memory;
	uint64_t *mm::m_data;
	std::set<void*> mm::m_malloced_ptrs; 
	std::map<void*, size_t> mm::m_mapped_ptrs; 


	mm_initializer::mm_initializer(){
		if ( 0 == nifty_counter++ ){
			// initialize static members object here
			// mm::m_items.clear();
			mm::m_total_memory = 0;
			mm::m_data = NULL;
		}
	}
	mm_initializer::~mm_initializer(){
		if ( 0 == --nifty_counter ){
			// clean up
		}
	}

	bool mm::map_hp(){
#ifdef MAP_HUGETLB		
		m_total_memory = 0; // memory of all int_vectors
		for(tMVecItem::const_iterator it=m_items.begin(); it!=m_items.end(); ++it){
			m_total_memory += it->second->size();
		}				
		if(util::verbose){
			std::cout<<"m_total_memory"<<m_total_memory<<std::endl;
		}
		size_t hpgs= (m_total_memory+HUGE_LEN-1)/HUGE_LEN; // number of huge pages required to store the int_vectors 
		m_data = (uint64_t*)mmap(NULL, hpgs*HUGE_LEN, HUGE_PROTECTION, HUGE_FLAGS, 0, 0);
		if (m_data == MAP_FAILED) {
			std::cout << "mmap was not successful" << std::endl;
			return false;
		}else{
			if( util::verbose ){
				std::cerr<<"map " << m_total_memory << " bytes" << std::endl; 
			}
		}
		// map int_vectors
		uint64_t *addr = m_data;
		bool success = true;
		for(tMVecItem::const_iterator it=m_items.begin(); it!=m_items.end(); ++it){
//			std::cerr<<"addr = "<< addr << std::endl;
			success = success && it->second->map_hp( addr );
		}
		return success;
#else
		return false;
#endif		
	}

	bool mm::unmap_hp(){
#ifdef MAP_HUGETLB		
		size_t hpgs= (m_total_memory+HUGE_LEN-1)/HUGE_LEN; // number of huge pages 
		if (  util::verbose ){
			std::cerr<<"unmap "<< m_total_memory << " bytes" <<std::endl;
			std::cerr.flush();
		}
		bool success = true;
		for(tMVecItem::const_iterator it=m_items.begin(); it!=m_items.end(); ++it){
			success = success && it->second->unmap_hp();
		}
//		uint64_t* tmp_data = (uint64_t*)malloc(m_total_memory); // allocate memory for int_vectors 
//		memcpy(tmp_data, m_data, len); // copy data from the mmapped region
		int ret = munmap((void*)m_data, hpgs*HUGE_LEN ); 
		if ( ret == -1 ){
			perror("Unmap failed");
			return false;
		}
		return success;
#else
		return true;
#endif
	}

	void* mm::malloc_hp(size_t size){
		bool success = true;
		return malloc_hp(size, true, success);
	}

	void* mm::malloc_hp(size_t size, bool force_alloc, bool &success){
#ifdef MAP_HUGETLB		
		size_t hpgs= (size+HUGE_LEN-1)/HUGE_LEN; // number of huge pages required to meet the request
		void *ptr = (uint64_t*)mmap(NULL, hpgs*HUGE_LEN, HUGE_PROTECTION, HUGE_FLAGS, 0, 0);
		if (ptr == MAP_FAILED) {
//			logger<<"mm::malloc_hp("<<size<<") could not allocate hugepages"<<endl;
			if ( force_alloc ){
				void* ptr = registered_malloc(size);
				success = (NULL != ptr) or (size==0);
				return ptr;
			}else{
				success = false;
				return NULL;
			}
		}else{
			success = true;
//			logger<<"mm::malloc_hp("<<size<<") was successful and allocated "<<hpgs<<" hugepages"<<endl;
			m_mapped_ptrs[ptr] = hpgs;
			return ptr;		
		}
#else
		if( force_alloc ){
			void* ptr = registered_malloc(size);
			success = (NULL != ptr) or (size==0);
			return ptr;
		}else{
			success = false;
			return NULL;
		}
#endif		
	}

	void * mm::registered_malloc(size_t size){
		void * ptr = malloc(size);		// allocate memory via 
		if ( NULL != ptr ){
			m_malloced_ptrs.insert(ptr);
		}
		return ptr;		
	}

	void mm::free_hp(void *ptr){
		if ( NULL !=  ptr ){
			//   if the pointer was allocated wiht malloc 
			if ( m_malloced_ptrs.find(ptr) != m_malloced_ptrs.end() ){
				free(ptr);
				m_malloced_ptrs.erase(ptr);
//			logger<<"mm::free_hp("<<ptr<<") freed non-hugepage memory"<<endl;
			}
#ifdef MAP_HUGETLB			
			else if ( m_mapped_ptrs[ptr] != m_mapped_ptrs.end() ){				
				size_t hpgs = m_mapped_ptrs[ptr];
				int ret = munmap((void*)m_data, hpgs*HUGE_LEN ); 
				if ( ret == -1 ){
					perror("Unmap failed");
				}else{
					m_mapped_ptrs.erase(ptr);
//			logger<<"mm::free_hp("<<ptr<<") freed hugepage memory successfully"<<endl;
				}
			}
#endif		
		}
	}

} // end namespace
