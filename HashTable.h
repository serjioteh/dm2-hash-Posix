#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <iostream>
#include <cstring>
#include <ctime>

enum class HashTableErrorType
{
    NoKey,
    NoMemory,
    NotExists
};

class HashTableError: public std::runtime_error
{
private:
    HashTableErrorType type;

public:
    HashTableError(HashTableErrorType _type, std::string message):
            runtime_error(message),
            type(_type)
    {}

    HashTableErrorType getType() const { return type; }
};

struct Node
{
    int key, value, ttl;
    time_t t_death;
    int n_prev, n_next;
};

class NodePtr
{
public:
    NodePtr(Node *base, int n_node) : p_base(base) { p_node = (n_node != -1) ? base + n_node : NULL; }

    int   get_num() { return get_num(*this); }

    void set_key(int key) { if (p_node) p_node->key = key; }
    void set_value(int value) { if (p_node) p_node->value = value; }
    void set_ttl(int ttl)
    {
        if (p_node)
        {
            p_node->t_death = time(NULL) + ttl;
            p_node->ttl = ttl;
        }
    }

    int get_key() const { return p_node->key; }
    int get_value() const { return p_node->value; }
    int get_ttl() const { return p_node->ttl; }

    void set_next(NodePtr ptr) { if (p_node) p_node->n_next = get_num(ptr); }

    NodePtr get_next() { return (p_node) ? NodePtr(p_base, p_node->n_next) : NodePtr(p_base, -1); }

    void set_prev(int n_node) { if (p_node) p_node->n_prev = n_node; }
    void set_prev(NodePtr ptr) { if (p_node) p_node->n_prev = get_num(ptr); }

    NodePtr get_prev() { return (p_node) ? NodePtr(p_base, p_node->n_prev) : NodePtr(p_base, -1); }

    bool operator ==(Node * ptr) { return (ptr == p_node); }
    bool operator ==(NodePtr *p) { return (p->p_node == p_node);}

    bool operator !=(Node * ptr) { return (ptr != p_node); }
    bool operator !=(NodePtr *p) { return (p->p_node != p_node);}

    bool is_null() { return (p_node == NULL); }
    bool is_expired() { return p_node->t_death < time(NULL); }

private:
    int get_num(NodePtr ptr) { return (ptr.p_node) ? static_cast<int>(ptr.p_node - p_base) : -1; }

private:
    Node *p_base, *p_node;
};

class HashTable
{
public:
    HashTable(void *shmaddr);

    static void shared_memory_set(void *shmaddr)
    {
        void *p_mem = shmaddr;

        int *p_table = static_cast<int *>(p_mem);
        for (int i = 0; i < HashTable::HASH_TABLE_SIZE; i++)
            p_table[i] = -1;
        p_table[HashTable::HASH_TABLE_SIZE] = 0;

        Node *p_data = reinterpret_cast<Node *>(&p_table[HashTable::HASH_TABLE_SIZE+1]);

        for (int i = 0; i < HashTable::BUFFER_SIZE; i++)
        {
            p_data[i].n_prev = (i > 0) ? i-1: -1;
            p_data[i].n_next = (i < HashTable::BUFFER_SIZE - 1) ? i+1: -1;
        }
    }

    void set(int key, int ttl, int value);
    int  get(int key);
    void del(int key);

    ~HashTable() { }

    int get_hash(int key) const { return static_cast<int>(std::hash<int>()(key) % HASH_TABLE_SIZE); }

protected:
    NodePtr find_node(int key) const;

    NodePtr get_node_free();
    void set_node_free(NodePtr p_node);

private:
    int get_num_free_node() const { return p_table[HASH_TABLE_SIZE]; }
    void set_num_free_node(int k) { p_table[HASH_TABLE_SIZE] = k; }

public:
    static const int NUM_SECTIONS = 8;

    static const size_t HASH_TABLE_SIZE = 256;
    static const size_t BUFFER_SIZE = 2048;

    static const size_t TOTAL_MEM_SIZE = (HASH_TABLE_SIZE + 1) * sizeof(int) + BUFFER_SIZE * sizeof(Node);

protected:
    void *p_mem;
    Node *p_data;
    int  *p_table;
};


#endif //HASHTABLE_H
