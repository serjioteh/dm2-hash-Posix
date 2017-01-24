#include "HashTable.h"

HashTable::HashTable(void *shmaddr)
{
    p_mem = shmaddr;
    p_table = static_cast<int *>(p_mem);
    p_data = reinterpret_cast<Node *>(&p_table[HASH_TABLE_SIZE+1]);
}

void HashTable::set(int key, int ttl, int value)
{
    NodePtr p_node = find_node(key);

    if (!p_node.is_null())
    {
        p_node.set_value(value);
        p_node.set_ttl(ttl);
    }
    else
    {
        int hash = get_hash(key);
        p_node = get_node_free();
        p_node.set_key(key);
        p_node.set_value(value);
        p_node.set_ttl(ttl);

        NodePtr p_next(p_data, p_table[hash]);
        p_node.set_next(p_next);
        p_next.set_prev(p_node);
        p_table[hash] = p_node.get_num();
    }
}

int HashTable::get(int key)
{
    NodePtr p_node = find_node(key);

    if (!p_node.is_null())
    {
        if (!p_node.is_expired())
        {
            p_node.set_ttl(p_node.get_ttl());
            return p_node.get_value();
        }
        else
        {
            set_node_free(p_node);

            throw HashTableError(
                    HashTableErrorType::NotExists,
                    "The element with such key is expired."
            );
        }
    }
    else
    {
        throw HashTableError(
                HashTableErrorType::NoKey,
                "There is no element with such key in hash-table."
        );
    }
}

void HashTable::del(int key)
{
    NodePtr p_node = find_node(key);

    if (!p_node.is_null())
    {
        if (!p_node.is_expired())
        {
            set_node_free(p_node);
        }
        else
        {
            set_node_free(p_node);

            throw HashTableError(
                    HashTableErrorType::NotExists,
                    "The element with such key is expired."
            );
        }
    }
    else
    {
        throw HashTableError(
                HashTableErrorType::NoKey,
                "There is no element with such key in hash-table."
        );
    }
}


NodePtr HashTable::find_node(int key) const
{
    NodePtr p_node(p_data, p_table[get_hash(key)]);
    while (!p_node.is_null() && p_node.get_key() != key)
        p_node = p_node.get_next();
    return p_node;
}

NodePtr HashTable::get_node_free()
{
    NodePtr p_node(p_data, get_num_free_node());

    if (p_node.is_null())
    {
        throw HashTableError(
                HashTableErrorType::NoMemory,
                "There is no more free memory left. Sorry!"
        );
    }

    NodePtr p_free = p_node.get_next();
    p_free.set_prev(-1);
    set_num_free_node(p_free.get_num());
    return p_node;
}

void HashTable::set_node_free(NodePtr p_node)
{
    auto p_prev = p_node.get_prev(), p_next = p_node.get_next();

    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        if (p_table[i] == p_node.get_num())
        {
            p_table[i] = p_next.get_num();
            break;
        }
    }

    p_prev.set_next(p_next);
    p_next.set_prev(p_prev);

    NodePtr p_free(p_data, get_num_free_node());
    p_free.set_prev(p_node);

    p_node.set_next(p_free);
    p_node.set_prev(-1);

    set_num_free_node(p_node.get_num());
}
