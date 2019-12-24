/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Map.h
*  PURPOSE:     Optimized Map implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// Since we have the AVLTree class and the optimized native heap allocator semantics,
// we can get to implement a Map aswell. Sometimes we require Maps, so we do not want to
// depend on the STL things that often come with weird dependencies to third-party modules
// (specifically on the Linux implementation).

#ifndef _EIR_MAP_HEADER_
#define _EIR_MAP_HEADER_

#include "eirutils.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"

#include "AVLTree.h"
#include "AVLTree.helpers.h"
#include "avlsetmaputil.h"

#include <type_traits>

// TODO: add object allocator support for eir::Map.

namespace eir
{

typedef GenericDefaultComparator MapDefaultComparator;

template <typename keyType, typename valueType, typename allocatorType, typename comparatorType = MapDefaultComparator>
struct Map
{
    // Make templates friends of each-other.
    template <typename, typename, typename, typename> friend struct Map;

    // Node inside this Map.
    struct Node
    {
        template <typename, typename, typename, typename> friend struct Map;
        template <typename structType, typename subAllocatorType, typename... Args> friend structType* eir::dyn_new_struct( subAllocatorType&, void*, Args&&... );
        template <typename structType, typename subAllocatorType> friend void eir::dyn_del_struct( subAllocatorType&, void*, structType* ) noexcept;

        // Get rid of the default things.
        inline Node( const Node& right ) = delete;
        inline Node( Node&& right ) = delete;

        inline Node& operator = ( const Node& ) = delete;
        inline Node& operator = ( Node&& right ) = delete;

        // The public access stuff.
        inline const keyType& GetKey( void ) const
        {
            return this->key;
        }

        inline valueType& GetValue( void )
        {
            return this->value;
        }

        inline const valueType& GetValue( void ) const
        {
            return this->value;
        }

    private:
        // Not available to things that just use Map.
        // But the friend class does have access.
        inline Node( keyType key, valueType value )
            : key( std::move( key ) ), value( std::move( value ) )
        {
            return;
        }
        inline ~Node( void ) = default;

        keyType key;
        valueType value;

        AVLNode sortedByKeyNode;
    };

    inline Map( void ) noexcept
    {
        // Really nothing to do, I swear!
        return;
    }

private:
    struct avlKeyNodeDispatcher
    {
        template <typename subFirstKeyType, typename subSecondKeyType>
        static AINLINE eCompResult compare_keys( const subFirstKeyType& left, const subSecondKeyType& right )
        {
            if ( comparatorType::is_less_than( left, right ) )
            {
                return eCompResult::LEFT_LESS;
            }
            if ( comparatorType::is_less_than( right, left ) )
            {
                return eCompResult::LEFT_GREATER;
            }

            return eCompResult::EQUAL;
        }

        static inline eCompResult CompareNodes( const AVLNode *left, const AVLNode *right )
        {
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );
            const Node *rightNode = AVL_GETITEM( Node, right, sortedByKeyNode );

            return compare_keys( leftNode->key, rightNode->key );
        }

        template <typename subKeyType>
        static inline eCompResult CompareNodeWithValue( const AVLNode *left, const subKeyType& right )
        {
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return compare_keys( leftNode->key, right );
        }
    };

    typedef AVLTree <avlKeyNodeDispatcher> MapAVLTree;

    static AINLINE void dismantle_node( Map *refMem, Node *theNode )
    {
        eir::dyn_del_struct <Node> ( refMem->data.allocData, refMem, theNode );
    }

    static AINLINE void release_nodes( Map *refMem, MapAVLTree& workTree )
    {
        // Clear all allocated Nodes.
        while ( AVLNode *avlSomeNode = workTree.GetRootNode() )
        {
            Node *someNode = AVL_GETITEM( Node, avlSomeNode, sortedByKeyNode );

            workTree.RemoveByNodeFast( avlSomeNode );

            dismantle_node( refMem, someNode );
        }
    }

    static AINLINE Node* NewNode( Map *refMem, MapAVLTree& workTree, keyType key, valueType value )
    {
        Node *newNode = eir::dyn_new_struct <Node> ( refMem->data.allocData, refMem, std::move( key ), std::move( value ) );

        try
        {
            // Link this node into our system.
            workTree.Insert( &newNode->sortedByKeyNode );
        }
        catch( ... )
        {
            // Could receive an exception if the comparator of Node does throw.
            eir::dyn_del_struct <Node> ( refMem->data.allocData, refMem, newNode );

            throw;
        }

        return newNode;
    }

    template <typename otherNodeType, typename otherAVLTreeType>
    static AINLINE MapAVLTree clone_key_tree( Map *refMem, otherAVLTreeType& right )
    {
        // We know that there cannot be two-same-value nodes, so we skip checking nodestacks.
        MapAVLTree newTree;

        typename otherAVLTreeType::diff_node_iterator iter( right );

        try
        {
            while ( !iter.IsEnd() )
            {
                otherNodeType *rightNode = AVL_GETITEM( otherNodeType, iter.Resolve(), sortedByKeyNode );

                NewNode( refMem, newTree, rightNode->key, rightNode->value );

                iter.Increment();
            }
        }
        catch( ... )
        {
            // Have to release all items again because one of them did not cooperate.
            release_nodes( refMem, newTree );

            throw;
        }

        return newTree;
    }

    // Now decide if we need an object-allocator.
    INSTANCE_SUBSTRUCTCHECK( is_object );

    static constexpr bool hasObjectAllocator = PERFORM_SUBSTRUCTCHECK( allocatorType, is_object );

public:
    template <typename... Args>
    inline Map( constr_with_alloc _, Args... allocArgs ) : data( 0, MapAVLTree(), std::forward <Args> ( allocArgs )... )
    {
        return;
    }

    inline Map( const Map& right )
        : data( 0, clone_key_tree <Node> ( this, right.data.avlKeyTree ) )
    {
        return;
    }

    template <typename otherAllocatorType>
    inline Map( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right )
        : data( 0, clone_key_tree <typename Map <keyType, valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlKeyTree ) )
    {
        return;
    }

    // WARNING: only move-construct if you know that allocatorType matches in both objects.
    inline Map( Map&& right ) noexcept
        : data( 0, std::move( right.data.avlKeyTree ), std::move( right.data ) )
    {
        return;
    }

    inline ~Map( void )
    {
        release_nodes( this, this->data.avlKeyTree );
    }

    inline Map& operator = ( const Map& right )
    {
        // Clone the thing.
        MapAVLTree newTree = clone_key_tree <Node> ( this, right.data.avlKeyTree );

        // Release the old.
        release_nodes( this, this->data.avlKeyTree );

        this->data.avlKeyTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    template <typename otherAllocatorType>
    inline Map& operator = ( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right )
    {
        // Clone the thing.
        MapAVLTree newTree =
            clone_key_tree <typename Map <keyType, valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlKeyTree );

        // Release the old.
        release_nodes( this, this->data.avlKeyTree );

        this->data.avlKeyTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    // WARNING: only move if allocatorType points to the same data-repository in both objects!
    inline Map& operator = ( Map&& right ) noexcept
    {
        this->data = std::move( right.data );

        // We need to manually move over field values.
        this->data.avlKeyTree = std::move( right.data.avlKeyTree );

        return *this;
    }

    // *** Management methods.

    // Sets a new value to the tree.
    // Overrides any Node that previously existed.
    inline void Set( const keyType& key, valueType value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );

            existingNode->value = std::move( value );
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlKeyTree, key, std::move( value ) );
    }

    inline void Set( keyType&& key, valueType value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );

            existingNode->value = std::move( value );
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlKeyTree, std::move( key ), std::move( value ) );
    }

    // Removes a specific node that was previously found.
    // The code must make sure that the node really belongs to this tree.
    inline void RemoveNode( Node *theNode )
    {
        this->data.avlKeyTree.RemoveByNodeFast( &theNode->sortedByKeyNode );

        dismantle_node( this, theNode );
    }

    // Erases any Node by key.
    template <typename queryType>
    inline void RemoveByKey( const queryType& key )
    {
        AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key );

        if ( avlExistingNode == nullptr )
            return;

        // Remove it.
        Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );

        RemoveNode( existingNode );
    }

    // Clears all keys and values from this Map.
    inline void Clear( void )
    {
        release_nodes( this, this->data.avlKeyTree );
    }

    // Returns the amount of keys/values inside this Map.
    inline size_t GetKeyValueCount( void ) const
    {
        size_t count = 0;

        typename MapAVLTree::diff_node_iterator iter( this->data.avlKeyTree );

        while ( !iter.IsEnd() )
        {
            count++;

            iter.Increment();
        }

        return count;
    }

    template <typename queryType>
    inline Node* Find( const queryType& key )
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename queryType>
    inline const Node* Find( const queryType& key ) const
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindNode( key ) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    // Special finding function that uses a comparison function instead of mere "is_less_than".
    template <typename comparisonCallbackType>
    inline Node* FindByCriteria( const comparisonCallbackType& cb )
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindAnyNodeByCriteria(
            [&]( AVLNode *left )
        {
            Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType>
    inline const Node* FindByCriteria( const comparisonCallbackType& cb ) const
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindAnyNodeByCriteria(
            [&]( const AVLNode *left )
        {
            const Node *leftNode = AVL_GETITEM( constNode, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType>
    inline Node* FindMinimumByCriteria( const comparisonCallbackType& cb )
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindMinimumNodeByCriteria(
            [&]( AVLNode *left )
        {
            Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType>
    inline const Node* FindMinimumByCriteria( const comparisonCallbackType& cb ) const
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindMinimumNodeByCriteria(
            [&]( const AVLNode *left )
        {
            const Node *leftNode = AVL_GETITEM( constNode, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType>
    inline Node* FindMaximumByCriteria( const comparisonCallbackType& cb )
    {
        if ( AVLNode *avlExistingNode = this->data.avlKeyTree.FindMaximumNodeByCriteria(
            [&]( AVLNode *left )
        {
            Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename comparisonCallbackType>
    inline const Node* FindMaximumByCriteria( const comparisonCallbackType& cb ) const
    {
        if ( const AVLNode *avlExistingNode = this->data.avlKeyTree.FindMaximumNodeByCriteria(
            [&]( const AVLNode *left )
        {
            const Node *leftNode = AVL_GETITEM( constNode, left, sortedByKeyNode );

            return cb( leftNode );
        }) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByKeyNode );
        }

        return nullptr;
    }

    template <typename queryType>
    inline valueType FindOrDefault( const queryType& key )
    {
        if ( auto *findNode = this->Find( key ) )
        {
            return findNode->GetValue();
        }

        return valueType();
    }

    // Returns true if there is nothing inside this Map/the AVL tree.
    inline bool IsEmpty( void ) const
    {
        return ( this->data.avlKeyTree.GetRootNode() == nullptr );
    }

    MAKE_SETMAP_ITERATOR( iterator, Map, Node, sortedByKeyNode, data.avlKeyTree, MapAVLTree );
    typedef const Node constNode;
    typedef const Map constMap;
    MAKE_SETMAP_ITERATOR( const_iterator, constMap, constNode, sortedByKeyNode, data.avlKeyTree, MapAVLTree );

    // Walks through all nodes of this tree.
    template <typename callbackType>
    inline void WalkNodes( const callbackType& cb )
    {
        iterator iter( this );

        while ( !iter.IsEnd() )
        {
            Node *curNode = iter.Resolve();

            cb( curNode );

            iter.Increment();
        }
    }

    // Support for the standard C++ for-each walking.
    struct end_std_iterator {};
    struct std_iterator
    {
        AINLINE std_iterator( iterator&& right ) : iter( std::move( right ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& right ) const    { return iter.IsEnd() == false; }

        AINLINE std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }
        AINLINE Node* operator * ( void )
        {
            return iter.Resolve();
        }

    private:
        iterator iter;
    };
    AINLINE std_iterator begin( void )              { return std_iterator( iterator( *this ) ); }
    AINLINE end_std_iterator end( void ) const      { return end_std_iterator(); }

    struct const_std_iterator
    {
        AINLINE const_std_iterator( const_iterator&& right ) : iter( std::move( right ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& right ) const    { return iter.IsEnd() == false; }

        AINLINE const_std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }
        AINLINE const Node* operator * ( void )
        {
            return iter.Resolve();
        }

    private:
        const_iterator iter;
    };
    AINLINE const_std_iterator begin( void ) const  { return const_std_iterator( const_iterator( *this ) ); }

    // Nice helpers using operators.
    inline valueType& operator [] ( const keyType& key )
    {
        if ( Node *findNode = this->Find( key ) )
        {
            return findNode->value;
        }

        return NewNode( this, this->data.avlKeyTree, key, valueType() )->value;
    }

    inline valueType& operator [] ( keyType&& key )
    {
        if ( Node *findNode = this->Find( key ) )
        {
            return findNode->value;
        }

        return NewNode( this, this->data.avlKeyTree, std::move( key ), valueType() )->value;
    }

    template <typename otherAllocatorType>
    inline bool operator == ( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right ) const
    {
        typedef typename Map <keyType, valueType, otherAllocatorType, comparatorType>::Node otherNodeType;

        return EqualsAVLTrees( this->data.avlKeyTree, right.data.avlKeyTree,
            [&]( const AVLNode *left, const AVLNode *right )
        {
            // If any key is different then we do not match.
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByKeyNode );
            const otherNodeType *rightNode = AVL_GETITEM( otherNodeType, right, sortedByKeyNode );

            if ( leftNode->GetKey() != rightNode->GetKey() )
            {
                return false;
            }

            // If any value is different then we do not match.
            if ( leftNode->GetValue() != rightNode->GetValue() )
            {
                return false;
            }

            return true;
        });
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const Map <keyType, valueType, otherAllocatorType, comparatorType>& right ) const
    {
        return !( operator == ( right ) );
    }

private:
    struct mutable_data
    {
        AINLINE mutable_data( void ) = default;
        AINLINE mutable_data( MapAVLTree tree ) : avlKeyTree( std::move( tree ) )
        {
            return;
        }
        AINLINE mutable_data( mutable_data&& right ) = default;

        AINLINE mutable_data& operator = ( mutable_data&& right ) = default;

        mutable MapAVLTree avlKeyTree;
    };

    size_opt <hasObjectAllocator, allocatorType, mutable_data> data;
};

}

#endif //_EIR_MAP_HEADER_
