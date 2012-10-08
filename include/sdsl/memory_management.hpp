/*!\file memory_management.hpp
   \brief memory_management.hpp contains two function for allocating and deallocating memory
   \author Your name
*/
#ifndef INCLUDED_MEMORY_MANAGEMENT 
#define INCLUDED_MEMORY_MANAGEMENT

#include "uintx_t.hpp"
#include "util.hpp"
#include <map>
#include <set>
#include <iostream>
using std::cout;
using std::endl;

namespace sdsl{
	
class mm_item_base{
	public:
		mm_item_base(){};
		virtual bool map_hp(uint64_t*&){return false;};// const = 0;
		virtual bool unmap_hp(){return false;};// const = 0;
		virtual ~mm_item_base(){};
		virtual uint64_t size(){return 0;};
};

template<class int_vector_type>
class mm_item : public mm_item_base{
	private:
		int_vector_type *m_v;
		bool m_mapped;		// indicated if the item is hugepage mapped or not
	public:
		explicit mm_item(int_vector_type *v):m_v(v), m_mapped(false){}
		~mm_item(){ }

		//! Map content of int_vector to a hugepage starting at address addr
		/*! 
		 *  Details: The content of the corresponding int_vector of mm_item 
		 *           is copied into a hugepage starting at address addr.
		 *           The string position in the hugepage is then increased
		 *           by the number of bytes used by the int_vector.
		 *           So addr is the new starting address for the next
		 *           mm_item which have to be mapped.
		 */
		bool map_hp(uint64_t*& addr){
			uint64_t len = size();
			if ( m_v->m_data != NULL ){
				memcpy((char*)addr, m_v->m_data, len); // copy old data
				free(m_v->m_data);
				m_v->m_data = addr;
				addr += (len/8);
				m_mapped = true;
			}
			return true;	
		}

		//! 
		bool unmap_hp(){
			if ( m_mapped ){
				uint64_t len = m_v->  size();
				uint64_t* tmp_data = (uint64_t*)malloc(len); // allocate memory for m_data
				memcpy(tmp_data, m_v->m_data, len); // copy data from the mmapped region
				m_v->m_data = tmp_data;
			}
			m_mapped = false;
			return true;
		}

		//! Returns the amount of memory required by the bit_vector of the item.
		uint64_t size(){
			return ((m_v->bit_size()+63)>>6)<<3;
		}

		//! Returns if the item is mapped to hugepage memory
		bool is_mapped(){
			return m_mapped;
		}
};

class mm_initializer; // forward declaration of initialization helper

// memory management class
class mm{
	friend class mm_initializer;
	typedef std::map<uint64_t, mm_item_base*> tMVecItem;
	static tMVecItem m_items; 
	static uint64_t m_total_memory; // memory of all int_vector in bytes 
	static uint64_t *m_data;
	static std::set<void*> m_malloced_ptrs;         // keeping track of results of
	static std::map<void*, size_t> m_mapped_ptrs;   // mm::malloc_hp 
	private:
		static void* registered_malloc(size_t size);
	public:
		mm();

		template<class int_vector_type>
		static void add(int_vector_type *v){
			if( mm::m_items.find((uint64_t)v) == mm::m_items.end() ){
				mm_item_base* item = new mm_item<int_vector_type>(v); 
				if(false and util::verbose) { 
					cout << "mm::add: add vector " << v << endl;
					cout.flush();
				}
				mm::m_items[(uint64_t)v] = item;
			}else{
				if(false and util::verbose) cout << "mm::add: mm_item is already in the set" << endl;
			}
		}

		template<class int_vector_type>
		static void remove(int_vector_type *v){
			if( mm::m_items.find((uint64_t)v) != mm::m_items.end() ){
				if( false and util::verbose ){ cout << "mm:remove: remove vector " << v << endl; };
				mm_item_base* item = m_items[(uint64_t)v];
				mm::m_items.erase((uint64_t)v);
				delete item;
			}else{
				if( false and util::verbose ){ cout << "mm:remove: mm_item is not in the set" << endl; };
			}
		}
		//! Map the heap memory of all registered object to hugepages
		/*!
		 * \sa unmap_hp 
		 */
		static bool map_hp();
		
		//! Unmap the hugepage-mapped memory of all registered objects
		/*!
		 * \sa map_hp 
		 */
		static bool unmap_hp();

		//! Maps a single int_vector to hugepage memory
		template<class int_vector_type>
		static bool map_hp(int_vector_type *v){
			// if the object is not yet registered
			if( mm::m_items.find((uint64_t)v) == mm::m_items.end() ){
				add(v);
			}
			if( mm::m_items.find((uint64_t)v) != mm::m_items.end() ){
				mm_item<int_vector_type>* item = (mm_item<int_vector_type>*)mm::m_items[(uint64_t)v];
				if ( !item->is_mapped() ){
					bool success = false;
					uint64_t *ptr = (uint64_t*) malloc_hp(item->size(), false, success);
					if ( success ){
						return item->map_hp( ptr );
					}
				}
				return true;
			}
			return false;
		}

		template<class int_vector_type>
		static bool unmap_hp(int_vector_type *v){
			if( mm::m_items.find((uint64_t)v) != mm::m_items.end() ){
				mm_item<int_vector_type>* item = (mm_item<int_vector_type>*)mm::m_items[(uint64_t)v];
				if ( item->is_mapped() ){
					return item->unmap_hp();
				}else{
					return false;
				}
			}else{
				return false;
			}
		}

		//! malloc_hp tries to allocate size bytes of hugepage memory
		/*! 
		 * \param size 	Size of requested memory in bytes.
	     * \return 		A pointer to the allocated memory. The memory
	     *         		manager tries to allocate the memory in hugepage
	     *         		segmented memory. However, if this fails 
		 * \sa free_hp
		 */
		static void* malloc_hp(size_t size);

		static void* malloc_hp(size_t size, bool force_alloc, bool &success);

		//! free_hp frees malloc_hp allocated memory
		/*! 
		 * \param ptr 		
		 * \sa malloc_hp 
	     */	  
		static void free_hp(void *ptr);	
};

static class mm_initializer{
	public:
		mm_initializer ();
		~mm_initializer ();
} initializer;

} // end namespace

#endif
