#include "mymalloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

/* C programları için kullılan dinamik bellek tahsisi ve yönetim işlevselliğini sağlayan "first fit", "best fit", "worst fit", "next fit" 
gibi farklı tahsis stratejileri uygulayan bir bellek yönetim uygulaması.   */

/** finds a block based on strategy,
 * if necessary it splits the block,
 * allocates memory,
 * returns the start addrees of data[]*/
void *mymalloc(size_t size) {
    
    if (free_list == NULL || free_list->info.size < size) {              /* İlk çağrı veya yeterli alan kontrolü yapılıyor */
        size_t alloc_size = size > HEAP_SIZE ? size : HEAP_SIZE;          
        heap_start = sbrk(alloc_size);                                   /* Heap genişletiliyor */
        if (heap_start == (void *)-1) {
            perror("sbrk error: not available memory");
            return NULL;
        }
        heap_end = (Block *)((char *)heap_start + alloc_size);
        free_list = heap_start;
        free_list->info.size = alloc_size - sizeof(Block);
        free_list->info.isfree = 1;
        free_list->next = NULL;                   /* Yeni oluşturulan tek serbest blok listesinin hem bir sonraki hem de bir önceki bloğu NULL atandı */
        free_list->prev = NULL;
    }

    
    size_t rounded_size = (size + 15) & ~(size_t)15;                /* Size 16'nın katı olacak şekilde yukarı yuvarlama yapıldı */

    Block *curr = free_list;                  
    Block *prev = NULL;
    void *ptr = NULL;

    switch (strategy) 
	{
        case BEST_FIT: 
            Block *best_fit_block = NULL;
            size_t min_size = HEAP_SIZE;
            while (curr != NULL) 
			{
                if (curr->info.size >= rounded_size && curr->info.isfree && curr->info.size < min_size) 
				{
                    best_fit_block = curr;          
                    min_size = curr->info.size;
                }
                prev = curr;
                curr = next_block_in_freelist(curr);
            }

            if (best_fit_block != NULL) 
			{
                if (best_fit_block->info.size > rounded_size + sizeof(Block)) 
				{
                    best_fit_block = split_block(best_fit_block, rounded_size);       /* Bulunan free block split fonksiyonuyla bölündü */
                }

                best_fit_block->info.isfree = 0;
                ptr = best_fit_block->data;
            }
            break;

        case NEXT_FIT: 
            // Önceki konumu belirle
            static Block *next_fit_ptr = NULL;
            if (next_fit_ptr == NULL) 
			{
                next_fit_ptr = free_list; // Başlangıçta free_list'den başla
            }

            Block *start_ptr_next = next_fit_ptr; // Başlangıç noktasını kaydet

            // Bir döngüyle bir sonraki uygun bloğu bul
            do {
                if (next_fit_ptr->info.size >= rounded_size && next_fit_ptr->info.isfree) 
				{
                    // Bloğu böl
                    if (next_fit_ptr->info.size > rounded_size + sizeof(Block)) {
                        next_fit_ptr = split_block(next_fit_ptr, rounded_size);
                    }

                    next_fit_ptr->info.isfree = 0;
                    ptr = next_fit_ptr->data;
                    break;
                }
                next_fit_ptr = next_block_in_freelist(next_fit_ptr);
            } while (next_fit_ptr != start_ptr_next); // Bir döngü tamamlanana kadar devam et

            // Bir sonraki konumu güncelle
            if (ptr != NULL) 
			{
                next_fit_ptr = next_block_in_freelist(next_fit_ptr);
                if (next_fit_ptr == NULL) 
				{
                    next_fit_ptr = free_list; // free_list'in sonuna gelindiğinde başa dön
                }
            }
            break;
            

        case FIRST_FIT: 
    	    Block *start_ptr = free_list; // Başlangıç noktasını kaydet

            while (start_ptr != NULL) 
			{
        		if (start_ptr->info.size >= rounded_size && start_ptr->info.isfree) 
				{
                   if (start_ptr->info.size > rounded_size + sizeof(Block)) 
				   {
                      start_ptr = split_block(start_ptr, rounded_size);
            	   }

                  start_ptr->info.isfree = 0;
                  ptr = start_ptr->data;
                  break;
        		}
            start_ptr = next_block_in_freelist(start_ptr);
            }
    		break;
    		
        case WORST_FIT:
            Block *worst_fit_block = NULL;
            size_t max_size = 0;
        	while (curr != NULL) 
			{
            	if (curr->info.size >= rounded_size && curr->info.isfree && curr->info.size > max_size) 
				{
                worst_fit_block = curr;
                max_size = curr->info.size;
            	}
              prev = curr;
              curr = next_block_in_freelist(curr);
        	}

        if (worst_fit_block != NULL) 
		{
            // Bloğu böl
            if (worst_fit_block->info.size > rounded_size + sizeof(Block)) 
			{
                worst_fit_block = split_block(worst_fit_block, rounded_size);
            }

            worst_fit_block->info.isfree = 0;
            ptr = worst_fit_block->data;
        }
        break;
            
    }

    return ptr;    /* Link listten çıkarılıp return edildi */
}

/** frees block,
 * if necessary it coalesces with negihbors,
 * adjusts the free list
 */
void myfree(void *p) {
    if (p == NULL)
	{
		 return;
	}

    Block *block = (Block *)((char *)p - offsetof(Block, data));
    block->info.isfree = 1;                                    /* Belleğin boş olduğunu gösterir */

    if (strategy == BEST_FIT || strategy == FIRST_FIT) 
	{        
        Block *coalesced = left_coalesce(block);               /* Birleştirme yapılan kısım */
        coalesced = right_coalesce(coalesced);
    }
   
    if (listtype == ADDR_ORDERED_LIST)                         /* Free listeye ekleme */
	{                       
        
        if (free_list == NULL || block < free_list)            /* Adres sırasına göre sıralı ekleme */
		{          
            block->next = free_list;
            if (free_list != NULL) 
			{
                free_list->prev = block;
            }
            free_list = block;
            block->prev = NULL;
        } 
        
		else 
		{
            Block *curr = free_list;
            while (curr->next != NULL && curr->next < block) 
			{
                curr = curr->next;
            }
            block->next = curr->next;
            block->prev = curr;
            if (curr->next != NULL) 
			{
                curr->next->prev = block;
            }
            curr->next = block;
        }
    } 
	
	else 
	{
        block->next = free_list;            /* LIFO son giren ilk çıkana göre ekleme */
        if (free_list != NULL) 
		{
            free_list->prev = block;
        }
        free_list = block;
        block->prev = NULL;
    }
}

/** splits block, by using the size(in 16 byte blocks)
 * returns the left block,
 * make necessary adjustments to the free list
 */
Block *split_block(Block *b, size_t size) {
    Block *new_block = (Block *)((char *)b + sizeof(Block) + size);
    new_block->info.size = b->info.size - size - sizeof(Block);
    new_block->info.isfree = 1;

    if (listtype == ADDR_ORDERED_LIST) 
	{
        new_block->next = b->next;
        new_block->prev = b;
        if (b->next != NULL) 
		{
            b->next->prev = new_block;
        }
        b->next = new_block;
    }

    b->info.size = size;

    return b;                /* New_block'un solunda bulunan b bloğu döndürdük */
}

/** coalesce b with its left neighbor
 * returns the final block
 */
Block *left_coalesce(Block *b) {
    if (b->prev != NULL && b->prev->info.isfree) 
	{
        b->prev->info.size += sizeof(Block) + b->info.size;
        b->prev->next = b->next;
        if (b->next != NULL) 
		{
            b->next->prev = b->prev;
        }
        b = b->prev;
    }

    return b;
}

/** coalesce b with its left neighbor
 * returns the final block
 */
Block *right_coalesce(Block *b) {
    if (b->next != NULL && b->next->info.isfree) 
	{
        b->info.size += sizeof(Block) + b->next->info.size;  
        b->next = b->next->next;
        if (b->next != NULL) 
		{
            b->next->prev = b;
        }
    }

    return b;
}

/** for a given block returns its next block in the list*/
Block *next_block_in_freelist(Block *b) {
    if (listtype == ADDR_ORDERED_LIST)        /* Sıralı listeye ekleme yapıyor */
	{           
        return b->next;
    } 
    
	else if (listtype == UNORDERED_LIST)      /* Srırasız listeye ekleme yapıyor */
	{
        Block *curr = free_list;
        while (curr != NULL && curr != b) 
		{
            curr = curr->next;
        }
        if (curr != NULL) 
		{
            return curr->next;
        }
    }
    return NULL; 
}

/** for a given block returns its prev block in the list*/
Block *prev_block_in_freelist(Block *b) {
    if (listtype == ADDR_ORDERED_LIST) 
	{
        return b->prev;
    } 
    
	else if (listtype == UNORDERED_LIST) 
	{
        Block *curr = free_list;
        while (curr != NULL && curr->next != b)     
		{
            curr = curr->next;
        }
        if (curr != NULL) 
		{
            return curr;
        }
    }
    return NULL;     /* List type ADDR_ORDERED_LIST veya UNORDERED_LIST dışında bir değer ise */
}

/** for a given block returns its right neghbor in the address*/
Block *next_block_in_addr(Block *b) {
    return (Block *)((char *)b + sizeof(Block) + b->info.size);
}

/** for a given block returns its left neghbor in the address*/
Block *prev_block_in_addr(Block *b) {
    return (Block *)((char *)b - b->prev->info.size - sizeof(Block));
}

/**for a given size in bytes, returns number of 16 blocks*/
uint64_t numberof16blocks(size_t size_inbytes) {
    return (size_inbytes + 15) / 16;
}

/** prints meta data of the blocks
 * --------
 * size:
 * free:
 * --------
 */
void printheap() {
    printf("Blocks\n");

    Block *curr = heap_start;
    while (curr != NULL && curr < heap_end)    /* Bellek aralığının kontrolünü yapıyoruz */
	{ 
        printf("Free: %d\n", curr->info.isfree);
        printf("Size: %" PRIu64 "\n", curr->info.size);       /* Uint64_t türünde değişkeni yazdırmak için kullandık */
        printf("--------------------------------\n");
        curr = next_block_in_addr(curr);
    }
}

/* Ayarlanan listeyi döndürüyor */
ListType getlisttype() { 
    return listtype; 
}

/* Listenin ayarlanması için kullanılıyor */
int setlisttype(ListType lt) { 
    listtype = lt; 
    return 0; 
}

/* Ayarlanan stratejiyi döndürüyor */
Strategy getstrategy() { 
    return strategy; 
}

/* Strateji ayarlaması için kullanılıyor */
int setstrategy(Strategy st) {             
    strategy = st; 
    return 0; 
} 

int main() {
    /* Best fit için free ve size çıktısı */
    setstrategy(BEST_FIT);
    void *ptr1 = mymalloc(16);
    void *ptr2 = mymalloc(32);
    printheap();
    myfree(ptr1);
    myfree(ptr2);

    /* Next fit için free ve size çıktısı */
    setstrategy(NEXT_FIT);
    void *ptr3 = mymalloc(16);
    void *ptr4 = mymalloc(32);
    printheap();
    myfree(ptr3);
    myfree(ptr4);
    
    /* First fit için free ve size çıktısı */
    setstrategy(FIRST_FIT);
    void *ptr5 = mymalloc(16);
    void *ptr6 = mymalloc(32);
    printheap();
    myfree(ptr5);
    myfree(ptr6);
    
    /* Worst fit için free ve size çıktısı */
    setstrategy(WORST_FIT);
    void *ptr7 = mymalloc(16);
    void *ptr8 = mymalloc(32);
    printheap();
    myfree(ptr7);
    myfree(ptr8);

    return 0;
}
