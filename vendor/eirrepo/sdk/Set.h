/*****************************************************************************
*
*  PROJECT:     Eir SDK
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        eirrepo/sdk/Set.h
*  PURPOSE:     Optimized Set implementation
*
*  Find the Eir SDK at: https://osdn.net/projects/eirrepo/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

// In the FileSystem presence data manager we need to determine an ordered set
// of locations that could be used for temporary data storage. Thus we need to
// offer a "Set" type, which is just a value-based Map.

#ifndef _EIR_SDK_SET_HEADER_
#define _EIR_SDK_SET_HEADER_

#include "eirutils.h"
#include "AVLTree.h"
#include "MacroUtils.h"
#include "MetaHelpers.h"

#include "AVLTree.helpers.h"
#include "avlsetmaputil.h"

namespace eir
{

typedef GenericDefaultComparator SetDefaultComparator;

template <typename valueType, typename allocatorType, typename comparatorType = SetDefaultComparator>
struct Set
{
    // Make templates friends of each-other.
    template <typename, typename, typename> friend struct Set;

    // Node inside this Set.
    struct Node
    {
        template <typename, typename, typename> friend struct Set;
        template <typename structType, typename subAllocatorType, typename... Args> friend structType* eir::dyn_new_struct( subAllocatorType&, void*, Args&&... );
        template <typename structType, typename subAllocatorType> friend void eir::dyn_del_struct( subAllocatorType&, void*, structType* ) noexcept;

        // Get rid of the default things.
        inline Node( const Node& right ) = delete;
        inline Node( Node&& right ) = delete;

        inline Node& operator = ( const Node& ) = delete;
        inline Node& operator = ( Node&& right ) = delete;

        // The public access stuff.
        inline const valueType& GetValue( void ) const
        {
            return this->value;
        }

    private:
        // Not available to things that just use Set.
        // But the friend class does have access.
        inline Node( valueType value )
            : value( std::move( value ) )
        {
            return;
        }
        inline ~Node( void ) = default;

        valueType value;

        AVLNode sortedByValueNode;
    };

    inline Set( void ) noexcept
    {
        // Really nothing to do, I swear!
        return;
    }

private:
    struct avlKeyNodeDispatcher
    {
        template <typename subFirstValueType, typename subSecondValueType>
        static AINLINE eCompResult compare_values( const subFirstValueType& left, const subSecondValueType& right )
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
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByValueNode );
            const Node *rightNode = AVL_GETITEM( Node, right, sortedByValueNode );

            return compare_values( leftNode->value, rightNode->value );
        }

        template <typename subValueType>
        static inline eCompResult CompareNodeWithValue( const AVLNode *left, const subValueType& right )
        {
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByValueNode );

            return compare_values( leftNode->value, right );
        }
    };

    typedef AVLTree <avlKeyNodeDispatcher> SetAVLTree;

    static AINLINE void dismantle_node( Set *refMem, Node *theNode )
    {
        eir::dyn_del_struct <Node> ( refMem->data.allocData, refMem, theNode );
    }

    static AINLINE void release_nodes( Set *refMem, SetAVLTree& workTree )
    {
        // Clear all allocated Nodes.
        while ( AVLNode *avlSomeNode = workTree.GetRootNode() )
        {
            Node *someNode = AVL_GETITEM( Node, avlSomeNode, sortedByValueNode );

            workTree.RemoveByNodeFast( avlSomeNode );

            dismantle_node( refMem, someNode );
        }
    }

    static AINLINE Node* NewNode( Set *refMem, SetAVLTree& workTree, valueType value )
    {
        Node *newNode = eir::dyn_new_struct <Node> ( refMem->data.allocData, refMem, std::move( value ) );

        try
        {
            // Link this node into our system.
            workTree.Insert( &newNode->sortedByValueNode );
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
    static AINLINE SetAVLTree clone_value_tree( Set *refMem, otherAVLTreeType& right )
    {
        // We know that there cannot be two-same-value nodes, so we skip checking nodestacks.
        SetAVLTree newTree;

        typename otherAVLTreeType::diff_node_iterator iter( right );

        try
        {
            while ( !iter.IsEnd() )
            {
                otherNodeType *rightNode = AVL_GETITEM( otherNodeType, iter.Resolve(), sortedByValueNode );

                NewNode( refMem, newTree, rightNode->value );

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
    inline Set( constr_with_alloc _, Args&&... allocArgs ) : data( 0, SetAVLTree(), std::forward <Args> ( allocArgs )... )
    {
        return;
    }

    inline Set( const Set& right )
        : data( 0, clone_value_tree <Node> ( this, right.data.avlValueTree ) )
    {
        return;
    }

    template <typename otherAllocatorType>
    inline Set( const Set <valueType, otherAllocatorType, comparatorType>& right )
        : data( 0, clone_value_tree <typename Set <valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlValueTree ) )
    {
        return;
    }

    // WARNING: only move-construct if you know that allocatorType matches in both objects.
    inline Set( Set&& right ) noexcept
        : data( 0, std::move( right.data.avlValueTree ), std::move( right.data ) )
    {
        return;
    }

    inline ~Set( void )
    {
        release_nodes( this, this->data.avlValueTree );
    }

    inline Set& operator = ( const Set& right )
    {
        // Clone the thing.
        SetAVLTree newTree = clone_value_tree <Node> ( this, right.data.avlValueTree );

        // Release the old.
        release_nodes( this, this->data.avlValueTree );

        this->data.avlValueTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    template <typename otherAllocatorType>
    inline Set& operator = ( const Set <valueType, otherAllocatorType, comparatorType>& right )
    {
        // Clone the thing.
        SetAVLTree newTree =
            clone_value_tree <typename Set <valueType, otherAllocatorType, comparatorType>::Node> ( this, right.data.avlValueTree );

        // Release the old.
        release_nodes( this, this->data.avlValueTree );

        this->data.avlValueTree = std::move( newTree );

        // TODO: think about copying the allocator aswell.

        return *this;
    }

    // WARNING: only move if allocatorType points to the same data-repository in both objects!
    inline Set& operator = ( Set&& right ) noexcept
    {
        this->data = std::move( right.data );

        // Need to manually move over fields.
        this->data.avlValueTree = std::move( right.data.avlValueTree );

        return *this;
    }

    // *** Management methods.

    // Sets a new value to the tree.
    // Overrides any Node that previously existed.
    inline void Insert( const valueType& value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlValueTree, value );
    }

    inline void Insert( valueType&& value )
    {
        // Check if such a node exists.
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return;
        }

        // We must create a new node.
        NewNode( this, this->data.avlValueTree, std::move( value ) );
    }

    // Removes a specific node that was previously found.
    // The code must make sure that the node really belongs to this tree.
    inline void RemoveNode( Node *theNode )
    {
        this->data.avlValueTree.RemoveByNodeFast( &theNode->sortedByValueNode );

        dismantle_node( this, theNode );
    }

    // Erases any Node by value.
    template <typename queryType>
    inline void Remove( const queryType& value )
    {
        AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value );

        if ( avlExistingNode == nullptr )
            return;

        // Remove it.
        Node *existingNode = AVL_GETITEM( Node, avlExistingNode, sortedByValueNode );

        RemoveNode( existingNode );
    }

    // Clears all values from this Set.
    inline void Clear( void )
    {
        release_nodes( this, this->data.avlValueTree );
    }

    // Returns the amount of values inside this Set.
    inline size_t GetValueCount( void ) const
    {
        size_t count = 0;

        typename SetAVLTree::diff_node_iterator iter( this->data.avlValueTree );

        while ( !iter.IsEnd() )
        {
            count++;

            iter.Increment();
        }

        return count;
    }

    template <typename queryType>
    inline Node* Find( const queryType& value )
    {
        if ( AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return AVL_GETITEM( Node, avlExistingNode, sortedByValueNode );
        }

        return nullptr;
    }

    template <typename queryType>
    inline const Node* Find( const queryType& value ) const
    {
        if ( const AVLNode *avlExistingNode = this->data.avlValueTree.FindNode( value ) )
        {
            return AVL_GETITEM( constNode, avlExistingNode, sortedByValueNode );
        }

        return nullptr;
    }

    // Returns true if there is nothing inside this Set/the AVL tree.
    inline bool IsEmpty( void ) const
    {
        return ( this->data.avlValueTree.GetRootNode() == nullptr );
    }

    // Public iterator.
    MAKE_SETMAP_ITERATOR( iterator, Set, Node, sortedByValueNode, data.avlValueTree, SetAVLTree );
    typedef const Node constNode;
    typedef const Set constSet;
    MAKE_SETMAP_ITERATOR( const_iterator, constSet, constNode, sortedByValueNode, data.avlValueTree, SetAVLTree );

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

    // Support for standard C++ container for-each loop.
    struct end_std_iterator {};
    struct std_iterator
    {
        AINLINE std_iterator( const_iterator&& right ) : iter( std::move( right ) )
        {
            return;
        }

        AINLINE bool operator != ( const end_std_iterator& ) const      { return iter.IsEnd() == false; }

        AINLINE std_iterator& operator ++ ( void )
        {
            iter.Increment();
            return *this;
        }
        AINLINE const valueType& operator * ( void )    { return iter.Resolve()->GetValue(); }

    private:
        const_iterator iter;
    };
    AINLINE std_iterator begin( void ) const    { return std_iterator( const_iterator( *this ) ); }
    AINLINE end_std_iterator end( void ) const  { return end_std_iterator(); }

    // TODO: create a common method between Map and Set so they reuse maximum amount of code.
    template <typename otherAllocatorType>
    inline bool operator == ( const Set <valueType, otherAllocatorType, comparatorType>& right ) const
    {
        typedef typename Set <valueType, otherAllocatorType, comparatorType>::Node otherNodeType;

        return EqualsAVLTrees( this->data.avlValueTree, right.data.avlValueTree,
            [&]( const AVLNode *left, const AVLNode *right )
        {
            // If any value is different then we do not match.
            const Node *leftNode = AVL_GETITEM( Node, left, sortedByValueNode );
            const otherNodeType *rightNode = AVL_GETITEM( otherNodeType, right, sortedByValueNode );

            return ( leftNode->GetValue() == rightNode->GetValue() );
        });
    }

    template <typename otherAllocatorType>
    inline bool operator != ( const Set <valueType, otherAllocatorType, comparatorType>& right ) const
    {
        return !( operator == ( right ) );
    }

private:
    struct mutable_data
    {
        AINLINE mutable_data( void ) = default;
        AINLINE mutable_data( SetAVLTree tree ) : avlValueTree( std::move( tree ) )
        {
            return;
        }
        AINLINE mutable_data( mutable_data&& right ) = default;

        AINLINE mutable_data& operator = ( mutable_data&& right ) = default;

        mutable SetAVLTree avlValueTree;
    };

    size_opt <hasObjectAllocator, allocatorType, mutable_data> data;
};

}

#endif //_EIR_SDK_SET_HEADER_
