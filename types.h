#define TYPE_TAG(name) int type_tag
#define TYPE_OF(ptr) (*((int *)(ptr)))
#define IS_TYPE(ptr, type) (TYPE_OF((ptr)) == (type))
