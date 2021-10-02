#include <iostream>
#include <unordered_map>
#include <map>
#include <climits>
#include <assert.h>
#include <fstream>
#include <string>
using namespace std;

struct Item {
	string key;
	int value;
	int priority; // 1-100
	int expireTime; // seconds, we assume it unique
	Item *prev, *next;
	Item(string k, int v, int p, int t): key(k), value(v), priority(p), expireTime(t), prev(nullptr), next(nullptr) {}
};

typedef unordered_map<string, Item*>::iterator MapIt;

class PECache {
public:
	static int TIME_LIMIT;
private:
	int maxCapacity;
	unordered_map<string, Item*> mItems;

	// sorted by expire time
	// so the expire time is unique?
	map<int, Item*> mExpireItems; 

	// sorted by priority
	// the second value is actually the head of the same priority items' loop list
	map<int, Item*> mPriorityItems; 

	void list_rebuild(int key, Item* oldItem) {
		// TODO single item list, next to itself?
		// make sure item exists outside
		auto head = mPriorityItems.at(key);
		auto tail = head->prev;

		// break the old link
		(oldItem->prev)->next = oldItem->next;
		(oldItem->next)->prev = oldItem->prev;

		// move the item to the end
		tail->next = oldItem;
		oldItem->prev = tail;
		oldItem->next = head;
		head->prev = oldItem;
	}

	void list_insert(int key, Item* newItem) {
		// make sure item exists outside
		auto head = mPriorityItems.at(key);
		auto tail = head->prev;

		// move the item to the end
		tail->next = newItem;
		newItem->prev = tail;
		newItem->next = head;
		head->prev = newItem;
	}

	void list_remove(int key, Item* oldItem) {
		// make sure item exists outside
		auto head = mPriorityItems.at(key);
		// the last item, just delete the list
		if (head->next == head)
		{
			assert(head == oldItem && "List remove item not exists!");
			mPriorityItems.erase(key);
			delete head;
			head = nullptr;
			return;
		}

		// break the old link
		(oldItem->prev)->next = oldItem->next;
		(oldItem->next)->prev = oldItem->prev;
		oldItem->prev = nullptr;
		oldItem->next = nullptr;
	}

	void list_push_back(int key, Item* newItem) {
		//cout<<"list_push_back "<<newItem->key<<endl;
		map<int, Item*>::iterator it = mPriorityItems.find(key);
		// new list
		if (it == mPriorityItems.end()) {
			//cout<<"not find priority "<<key<<endl;
			mPriorityItems.insert({ key, newItem });
			newItem->next = newItem;
			newItem->prev = newItem;
		}
		else {
			auto head = mPriorityItems.at(key);
			auto tail = head->prev;
			//cout<<"list_push_back "<<newItem->key<<" after "<<tail->key<<endl;

			// move the item to the end
			tail->next = newItem;
			newItem->prev = tail;
			newItem->next = head;
			head->prev = newItem;
		}
	}

public:
	PECache(int capacity) :maxCapacity(capacity) {

	}

	Item* get(string key) {
		// find item if it exists
		MapIt it = mItems.find(key);
		if (it == mItems.end())
			return nullptr;

		Item* item = it->second;
		// make it most recently used
		list_rebuild(item->priority, item);

		return item;
	}

	void set(Item* item) {
		// if exists just do update
		MapIt it = mItems.find(item->key);
		if (it != mItems.end()) {
			Item* oldItem = it->second;
			if (oldItem->expireTime == item->expireTime and oldItem->priority == item->priority) {
				// no data update, just update the priority list
				list_rebuild(oldItem->priority, oldItem);
			}
			else {
				if (oldItem->expireTime != item->expireTime)
					mExpireItems.erase(oldItem->expireTime);
				
				// update the expire time map
				oldItem->expireTime = item->expireTime;
				mExpireItems.insert({ item->expireTime, oldItem });

				if (oldItem->priority != item->priority)
				{
					list_remove(oldItem->priority, oldItem);
					oldItem->priority = item->priority;
					list_insert(oldItem->priority, oldItem);
				}
				else
					list_rebuild(oldItem->priority, oldItem);
			}
		}
		else {
			list_push_back(item->priority, item);
			mItems.insert({ item->key, item });
			mExpireItems.insert({ item->expireTime, item });
		}
	}

	void set_capacity(int capacity) {
		// size overflow need delete items
		Item* expiredItem = nullptr;
		if (capacity < maxCapacity) {
			while (mItems.size() > capacity) {
				map<int, Item*>::iterator it = mExpireItems.begin();

				// some item expired
				if (it->first <= TIME_LIMIT) {
					expiredItem = it->second;
				}
				else {
					// delete the lowest priority and least recently used item
					map<int, Item*>::iterator it = mPriorityItems.begin();
					expiredItem = it->second;
				}

				if (expiredItem != nullptr) {
					mItems.erase(expiredItem->key);
					mExpireItems.erase(expiredItem->expireTime);
					list_remove(expiredItem->priority, expiredItem);
					// delete the invalid item
					delete expiredItem;
					expiredItem = nullptr;
				}
			}
		}
		maxCapacity = capacity;
	}

	void empty() {
		for(MapIt it = mItems.begin(); it != mItems.end(); ++it)
		{
			Item* item = it->second;
			delete item;
			item = nullptr;
		}
	}

	void build_from_file(string filename = "data.txt") {
		ifstream file;
		file.open(filename, ios::in);
		string line;

		if(file.is_open())
		{
			while (getline(file, line))
			{
				cout<<line<<endl;
			}
		}
	}

	void print() {
		cout<<"mItems:\n"<<"========================================"<<endl;
		for(MapIt it = mItems.begin(); it != mItems.end(); ++it)
		{
			Item* item = it->second;
			cout<<item->key<<" "<<item->value<<" "<<item->priority<<" "<<item->expireTime<<endl;
		}
		cout<<"mExpireItems:\n"<<"========================================"<<endl;
		for(map<int, Item*>::iterator it = mExpireItems.begin(); it != mExpireItems.end(); ++it)
		{
			Item* item = it->second;
			cout<<"expire time: "<<it->first<<" data => "<<item->key<<" "<<item->value<<" "<<item->priority<<" "<<item->expireTime<<endl;
		}
		cout<<"mPriorityItems:\n"<<"========================================"<<endl;
		for(map<int, Item*>::iterator it = mPriorityItems.begin(); it != mPriorityItems.end(); ++it)
		{
			Item* head = it->second;
			cout<<"priority: "<<it->first<<endl;
			Item* index = head;
			do {
				cout<<"			"<<index->key<<" "<<index->value<<" "<<index->priority<<" "<<index->expireTime<<endl;
				index = index->next;
			} while (index != head);
		}
	}
};

int PECache::TIME_LIMIT = INT_MAX;

int main() {
	PECache mPECache(5);
	mPECache.build_from_file();

	mPECache.set(new Item{"A", 12, 5, 11});
	mPECache.set(new Item{"B", 12, 4, 12});
	mPECache.set(new Item{"C", 12, 5, 13});
	mPECache.set(new Item{"D", 12, 5, 14});
	mPECache.set(new Item{"E", 12, 6, 15});

	mPECache.print();
	mPECache.empty();
}