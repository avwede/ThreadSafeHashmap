#include <iostream>
#include <cstddef>
#include <mutex>
#include <atomic>
#include "MarkableReference.h"

// Linked list abstract
template<class T>
class ILinkedList {
public:
	// Add an element to the list
	virtual void add(const T &val) = 0;

	// Find an element, return containment
	// Stores found element in val
	virtual bool find(T &val) = 0;
};

namespace ll {

	// Full support lock free ll
	template<class T>
	class LockFreeLL : ILinkedList<T> {
	private:
		// Regular Linked-List Node
		class Node {
		public:
			MarkableReference<Node> next;
			T val;

			Node() {} // Dummy node for head
			Node(T val) : val(val) {}
		};

		// Typedef to condense code later
		typedef MarkableReference<Node> MNode;

		MNode head;
		std::atomic<size_t> curSize;

	public:
		// Construct with dummy head
		LockFreeLL() : head(new Node()), curSize(0) {}

		// Destruct, free all nodes
		virtual ~LockFreeLL() {
			MNode curr = head;
			while (curr.getRef() != nullptr) {
				MNode next = curr.getRef()->next;
				delete curr.getRef();
				curr = next;
			}
		}

		// Returns the head. Not thread safe.
		MNode NOT_THREAD_SAFE_getHead() { return head; }

		void safeDelete(Node *toDelete) {
			// TODO: impl
		}


		// Find a value, internal use
		std::pair<MNode, MNode> _find(const T &val) {
			MNode pred, curr, succ;

			// Start at beginning of list
			retry: while (true) {
				// Head is guaranteed to exist, dummy node
				pred = head;
				curr = pred.getRef()->next;

				// While we have yet to reach the end of the list
				while (curr.getRef() != nullptr) {
					succ = curr.getRef()->next;

					while (succ.getMark()) {
						// Try to physically delete the logically deleted node
                        Node *expectedRef = curr.getRef();
                        bool expectedMark = false;
                        Node *requiredRef = succ.getRef();
                        bool requiredMark = false;

						if (!(pred.getRef()->next.compareExchangeBothWeak(
							expectedRef,
							expectedMark,
							requiredRef,
							requiredMark
						))) goto retry;

						// Move and free mem
						Node *toDelete = curr.getRef();

						curr = curr.getRef()->next;
						succ = succ.getRef()->next;

						safeDelete(toDelete);
					}

					// If we found it, return
					if (curr.getRef()->val == val)
						return { pred, curr };

					// Move
					pred = curr;
					curr = succ;
				}

				// Wasn't found, null curr
				return { pred, curr };
			}
		}

		// Add item to list
		void add(const T &val) {
			Node *toBeAdded = new Node(val);

			while (true) {
				// Find our val
				auto [ pred, curr ] = _find(val);

				// Item already exists
				if (curr.getRef() != nullptr &&
					curr.getRef()->val == val) return;

				// Attempt to link it in with CAS
				MNode node(toBeAdded);
				node.getRef()->next = curr;

                Node *expectedRef = curr.getRef();
                bool expectedMark = false;
                Node *requiredRef = node.getRef();
                bool requiredMark = false;

                if (pred.getRef()->next.compareExchangeBothWeak(
                    expectedRef,
                    expectedMark,
                    requiredRef,
                    requiredMark
                )) {
                    curSize++;
                    return;
                }
			}
		}

		// Remove item from list
		bool remove(const T &val) {
			while (true) {
				auto [ pred, curr ] = _find(val);

				// We didn't find it, stop
				if (curr.getRef() == nullptr ||
					curr.getRef()->val != val) return false;

				// Logically delete node by marking it's successor
				MNode succ = curr.getRef()->next;

				Node *expectedRef = succ.getRef();
				bool expectedMark = false;
				Node *requiredRef = succ.getRef();
				bool requiredMark = true;

				// Logicall deletion, might need to retry
				if (!(curr.getRef()->next.compareExchangeBothWeak(
					expectedRef,
					expectedMark,
					requiredRef,
					requiredMark
				))) continue;

				// It worked, attempt physical!
				Node *toDelete = curr.getRef();
				expectedRef = curr.getRef();
				expectedMark = false;
				requiredRef = succ.getRef();
				requiredMark = false;

				// If cut was successful, free mem
				// If it doesn't work immediately, don't worry about it
				// _find will clean
				if (pred.getRef()->next.compareExchangeBothWeak(
					expectedRef,
					expectedMark,
					requiredRef,
					requiredMark
				)) safeDelete(toDelete);

				curSize--;
				return true;
			}
		}

		// Returns true if the item is in the list,
		// parameter updated
		bool find(T &val) {
			auto [ _, curr ] = _find(val);
			if (curr.getRef() != nullptr &&
				curr.getRef()->val == val) {
				val = curr.getRef()->val;
				return true;
			}

			return false;
		}

		// Get current size
		size_t size() { return curSize; }
	};

	// Lock free linked list
	// No support for deletion
	template<class T>
	class AddOnlyLockFreeLL : ILinkedList<T> {
	private:
		// Regular linked list node
		struct Node {
			std::atomic<Node *> next = nullptr;
			T val;

			Node() {} // Dummy node for head
			Node(T val) : val(val) {}
		};

		// Size and head
		std::atomic_size_t curSize;
		Node *head;

	public:
		// Construct
		AddOnlyLockFreeLL() : curSize(0) {
			head = new Node();
		}

		// Free everything
		virtual ~AddOnlyLockFreeLL() {
			Node *curr = head;
			while (curr != nullptr) {
				Node *toRemove = curr;
				curr = curr->next;
				delete toRemove;
			}
		}

		// Get the head, not thread safe
		Node *NOT_THREAD_SAFE_getHead() { return head; }

		// Add new element to the list
		void add(const T &val) {
			Node *toAdd = new Node(val);

			// Keep going till we find success
			while (true) {
				Node *pred = head, *curr = head->next;

				while (curr != nullptr) {
					// Found it, update
					if (curr->val == val) {
						curr->val = val;
						delete toAdd;
						return;
					}

					pred = curr;
					curr = curr->next;
				}

				// Connect new node
				toAdd->next = curr;

				// Add with CAS
				Node *expected = curr;
				Node *required = toAdd;
				if (pred->next.compare_exchange_weak(
					expected,
					required
				)) {
					curSize++;
					return;
				}
			}
		}

		bool find(T &val) {
			Node *curr = head->next;

			while (curr != nullptr) {
				// Found it
				if (curr->val == val) {
					val = curr->val;
					return true;
				}

				curr = curr->next;
			}

			return false;
		}

		size_t size() { return curSize; }
	};

	// Hand over hand locked linked list
	template<class T>
	class LockableLL : ILinkedList<T> {
	private:
		// Lockable linked-list node
		struct LockableNode {
		private:
			// Lock
			std::mutex mtx;

		public:
			// Member variables
			LockableNode *next = nullptr;
			T val;

			// Construct
			LockableNode() {} // Dummy node for head
			LockableNode(T val) : val(val) {}

			// Wrappers for thread control
			void lock() { mtx.lock(); }
			void unlock() { mtx.unlock(); }

			// Lock the next node and return it
			LockableNode *getNextAndLock() {
				if (next == nullptr)
					return nullptr;
				next->mtx.lock();
				return next;
			}
		};

		// Member variables
		LockableNode *head = new LockableNode();
		std::atomic_size_t curSize;

	public:
		// Construct a new Linked-List
		LockableLL() : curSize(0) {}

		// Destructor, free all nodes
		virtual ~LockableLL() {
			// Obtain lock on head
			LockableNode *mover = head;
			mover->lock();

			// Free everything
			while (mover != nullptr) {
				LockableNode *next = mover->getNextAndLock();

				mover->unlock();
				delete mover;

				mover = next;
			}
		}

		// Returns the head. Not thread safe
		LockableNode *NOT_THREAD_SAFE_getHead() { return head; }

		// Add new element to the linked list
		void add(const T &val) {
			// Maintain lock on cur node
			LockableNode *mover = head;
			mover->lock();

			// Traverse the list
			bool isHead = true;
			while (true) {
				// If we find it, update
				if (!isHead && mover->val == val) {
					mover->val = val;
					mover->unlock();
					return;
				}

				// Lock our next node (if it exists)
				LockableNode *next = mover->getNextAndLock();
				// Stop if we've reached the end
				if (next == nullptr)
					break;

				// Unlock and move
				mover->unlock();
				mover = next;
				isHead = false;
			}

			// Insert at end
			mover->next = new LockableNode(val);
			mover->unlock();
			curSize++;
		}

		// Remove an element
		// Returns whether or not the value was successfully removed
		bool remove(T val) {
			// Maintain lock on current node
			LockableNode *mover = head;
			mover->lock();

			// Traverse, look for node to remove
			while (true) {
				// Get the next node
				LockableNode *next = mover->getNextAndLock();

				// Break if we're done
				if (next == nullptr) {
					mover->unlock();
					return false;
				}

				// Found it, remove
				if (next->val == val) {
					mover->next = next->next;
					mover->unlock();
					next->unlock();

					delete next;
					curSize--;

					return true;
				}

				// Unlock and move
				mover->unlock();
				mover = next;
			}

			mover->unlock();
			return false;
		}

		// Return existence, store val in param
		bool find(T &val) {
			// Empty list
			if (head->next == nullptr)
				return false;

			// Maintain lock on current node
			LockableNode *mover = head->next;
			mover->lock();

			// Check for existence
			while (true) {
				if (mover->val == val) {
					val = mover->val;
					mover->unlock();
					return true;
				}

				// Get next and break if done
				LockableNode *next = mover->getNextAndLock();

				if (next == nullptr) {
					mover->unlock();
					return false;
				}

				// Move
				mover->unlock();
				mover = next;
			}
		}

		// Get the current size
		size_t size() { return curSize; }
	};
};
