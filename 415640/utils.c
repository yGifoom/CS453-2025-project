#include <dict.h>
#include <macros.h>
#include <stdbool.h>
#include <tx_t.h>

int nested_free_dict(void *key, int unused(count), void* *value, void unused(*user)){
    if(*value != NULL){
        free(*value);
    }
    if (key != NULL){
        free(key);
    }
    return 0;
}


int add_from_dict(void *key, int unused(count), void* *value, void *user){
    struct dictionary* dict = (struct dictionary*)user;

    int res_add = dic_add(dict, key, 8);
    if(res_add != 1){
        *dict->value = *value;
    }
    return 0;
}

int rm_from_dict(void *key, int unused(count), void* unused(*value), void *user){
    struct dictionary* dict = (struct dictionary*)user;
    
    if(dic_find(dict, key, 8) == 1){
        dic_add(dict, key, 8);
        free(*dict->value);
        free(key);
        *dict->value = NULL;
    }
    return 0;
}

void dic_nested_destroy(struct dictionary* dic){
    dic_forEach(dic, nested_free_dict, NULL);
    dic_delete(dic);
}

void tx_destroy(transaction_t* tx, bool committed){
    if (!committed){
        dic_nested_destroy(tx->write_set);
        dic_nested_destroy(tx->read_set);
        dic_nested_destroy(tx->alloc_set);
        dic_nested_destroy(tx->free_set);
    }else{
        dic_delete(tx->write_set);
        dic_delete(tx->read_set);
        dic_delete(tx->alloc_set);
        dic_delete(tx->free_set);
    }
    
    free(tx);
    return;
}
