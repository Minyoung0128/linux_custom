#include <linux/xarray.h>
#include <linux/module.h>
#include <linux/slab.h>

static DEFINE_XARRAY(array); // xarray 구조체 정의

static void xa_check(const struct xarray *xa)
{
    void *entry = xa->xa_head;
    unsigned int shift = 0;

    pr_info("xarray : %px head %px flags %x marks %d %d %d\n", xa, entry, xa->xa_flags, xa_marked(xa, XA_MARK_0), xa_marked(xa, XA_MARK_1),xa_marked(xa, XA_MARK_2));
}

static int __init xarray_test(void)
{
    unsigned long i;

    void* ret;

    struct item{
        unsigned long index;
        unsigned int order;
    };

    pr_info("xarray_test() starting......\n");

    struct item* item;
    item = kmalloc(sizeof(*item),GFP_KERNEL);
    item->index = 0;
    item->order = 100;

    pr_info("[0] item = %p\n", item);

    ret = xa_store(&array, 0, (void*)item, GFP_KERNEL); // xarray index 0번에 저장

    for (i = 1; i <= 10; i++) { ///10번 반복 수행

        item = kmalloc(sizeof(*item), GFP_KERNEL); ///사용자 데이터 구조체 메모리 할당

        pr_info("[%d] item=%p, ", i, item);

        item->index = i;

        item->order = i+100;

        ret = xa_store(&array, i, (void*)item, GFP_KERNEL); ///xarray 인데스 i에 저장

        pr_info("ret=%p\n", ret); ///반환 주소 확인

        xa_check(&array); ///xarray 데이터 출력

    }

    ret = xa_load(&array, 0); ///xarray 인덱스 0에 있는 내용 가져옴.

    pr_info("load ret=%p, %d, %d\n",ret, ((struct item *)ret)->index, ((struct item *)ret)->order); ///가져온 내용 출력

    ret = xa_load(&array, 8); /// xarray 인데스 8에 있는 내용 가져옴

    pr_info("load ret=%p, %d, %d\n",

    ret, ((struct item *)ret)->index, ((struct item *)ret)->order); 
    ret = xa_erase(&array, 4); /// xarray 인덱스 4에 있는 내용 삭제

    pr_info("erase ret=%p, %d, %d\n", ret, ((struct item *)ret)->index, ((struct item *)ret)->order);

    ret = xa_erase(&array, 7); /// xarray 인덱스 7에 있는 내용 삭제

    pr_info("erase ret=%p, %d, %d\n", ret, ((struct item *)ret)->index, ((struct item *)ret)->order);

    

    i = 9;

    ret = xa_find(&array, &i, ULONG_MAX, XA_PRESENT); /// xarray 인덱스 9에 있는 검색

    pr_info("find ret=%p, %d, %d\n", ret, ((struct item *)ret)->index, ((struct item *)ret)->order);

    

    xa_for_each(&array, i, ret) { /// xarray 순환 탐색

        pr_info("each ret=%p, %d, %d\n", ret, ((struct item *)ret)->index, ((struct item *)ret)->order); ///탐색한 내용 출력

    }

    xa_destroy(&array); /// xarray 삭제

    pr_info("xarray_user_test() end.\n");

    return -EINVAL;
            
}

static void __exit xarray_exit(void)

{

    pr_info("xarray_exit() end.\n");

}

 

module_init(xarray_test);
module_exit(xarray_exit);
MODULE_AUTHOR("JaeJoon Jung <rgbi3307@nate.com>");
MODULE_LICENSE("GPL");