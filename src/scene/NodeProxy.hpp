#ifndef HYPERION_V2_NODE_PROXY_HPP
#define HYPERION_V2_NODE_PROXY_HPP

#include <core/Containers.hpp>
#include <GameCounter.hpp>
#include "Entity.hpp"

#include <math/Transform.hpp>
#include <math/Ray.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;
class Scene;
class Node;

class NodeProxy;

struct NodeProxyChildren {
    struct Iterator {
        Node *node;
        SizeType index;

        NodeProxy &operator*();
        const NodeProxy &operator*() const;

        NodeProxy *operator->();
        const NodeProxy *operator->() const;

        // postfix ++ operator
        Iterator &operator++()
        {
            ++index;

            return *this;
        }

        bool operator==(const Iterator &other) const
            { return node == other.node && index == other.index; }

        bool operator!=(const Iterator &other) const
            { return node != other.node || index != other.index; }
    };

    Node *node;

    SizeType Size() const;

    Iterator Begin() { return Iterator { node, 0 }; }
    const Iterator Begin() const { return const_cast<NodeProxyChildren *>(this)->Begin(); }
    Iterator End();
    const Iterator End() const { return const_cast<NodeProxyChildren *>(this)->End(); }

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    const Iterator cbegin() { return Begin(); }
    const Iterator cend() { return End(); }
};

class NodeProxy {
public:
    NodeProxy();
    NodeProxy(Node *node);
    NodeProxy(const NodeProxy &other);
    NodeProxy &operator=(const NodeProxy &other);
    NodeProxy(NodeProxy &&other) noexcept;
    NodeProxy &operator=(NodeProxy &&other) noexcept;
    ~NodeProxy();

    Node *Get() { return m_node; }
    const Node *Get() const { return m_node; }

    /*! \brief If the Node exists, returns the String name of the Node.
        Otherwise, returns the empty String. */
    const String &GetName() const;

    bool Any() const { return m_node != nullptr; }

    /*! \brief Conversion operator to bool, to use in if-statements */
    explicit operator bool() const { return Any(); }

    bool operator !() const { return !Any(); }

    /*! \brief Conversion operator to Node *. May be nullptr. */
    // operator Node *() const { return m_node; }

    bool operator==(const NodeProxy &other) const
        { return m_node == other.m_node; }

    bool operator!=(const NodeProxy &other) const
        { return m_node != other.m_node; }

    /*! \brief If the NodeProxy is not empty, and index is not out of bounds,
        returns a new NodeProxy, holding a pointer to the child of the held Node at the given index.
        If the index is out of bounds, returns an empty node proxy.
        If no Node is held, returns an empty NodeProxy. */
    NodeProxy GetChild(SizeType index);

    NodeProxyChildren GetChildren() { return NodeProxyChildren { m_node }; }
    const NodeProxyChildren GetChildren() const { return NodeProxyChildren { m_node }; }

    /*! \brief Search for a (potentially nested) node using the syntax `some/child/node`.
     * Each `/` indicates searching a level deeper, so first a child node with the tag "some"
     * is found, after which a child node with the tag "child" is searched for on the previous "some" node,
     * and finally a node with the tag "node" is searched for from the above "child" node.
     * If any level fails to find a node, nullptr is returned.
     *
     * The string is case-sensitive.
     * The '/' can be escaped by using a '\' char.
     */
    NodeProxy Select(const char *selector) const;

    /*! \brief If the Node is present, adds a new child to that Node,
        and returns a new NodeProxy of that added child Node. If the Node
        is not present, returns an empty NodeProxy. */
    NodeProxy AddChild();

    /*! \brief If the Node is present, adds the given child to the Node,
        and returns a new NodeProxy of that added child Node. If the Node
        is not present, returns an empty NodeProxy. */
    NodeProxy AddChild(const NodeProxy &node);

    /*! @returns The local-space translation, scale, rotation of this Node. */
    const Transform &GetLocalTransform() const;

    /*! @returns The local-space translation of this Node. */
    const Vector3 &GetLocalTranslation() const;

    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    void SetLocalTranslation(const Vector3 &translation);
    
    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * @param translation The vector to translate this Node by
     */
    void Translate(const Vector3 &translation)
        { SetLocalTranslation(GetLocalTranslation() + translation); }

    /*! @returns The local-space scale of this Node. */
    const Vector3 &GetLocalScale() const;

    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    void SetLocalScale(const Vector3 &scale);

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * @param scale The vector to scale this Node by
     */
    void Scale(const Vector3 &scale)
        { SetLocalScale(GetLocalScale() * scale); }
    
    /*! @returns The local-space rotation of this Node. */
    const Quaternion &GetLocalRotation() const;

    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    void SetLocalRotation(const Quaternion &rotation);

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * @param rotation The quaternion to rotate this Node by
     */
    void Rotate(const Quaternion &rotation)
        { SetLocalRotation(GetLocalRotation() * rotation); }

    /*! @returns The world-space translation, scale, rotation of this Node. Influenced by accumulative transformation of all ancestor Nodes. */
    const Transform &GetWorldTransform() const;

    /*! @returns The world-space translation of this Node. */
    const Vector3 &GetWorldTranslation() const;

    /*! \brief Set the world-space translation of this Node by offsetting the local-space translation */
    void SetWorldTranslation(const Vector3 &translation);

    /*! @returns The local-space scale of this Node. */
    const Vector3 &GetWorldScale() const;

    /*! \brief Set the local-space scale of this Node by offsetting the local-space scale */
    void SetWorldScale(const Vector3 &scale);

    /*! @returns The world-space rotation of this Node. */
    const Quaternion &GetWorldRotation() const;

    /*! \brief Set the world-space rotation of this Node by offsetting the local-space rotation */
    void SetWorldRotation(const Quaternion &rotation);

    /*! @returns The local-space (model) of the node's aabb. Only includes
     * the Entity's aabb.
     */
    const BoundingBox &GetLocalAABB() const;

    /*! @returns The world-space aabb of the node. Includes the transforms of all
     * parent nodes.
     */
    const BoundingBox &GetWorldAABB() const;

private:
    void ReleaseNode();

    Node *m_node;
};

} // namespace hyperion::v2

#endif