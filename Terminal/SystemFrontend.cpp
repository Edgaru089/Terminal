
#include "SystemFrontend.hpp"

using namespace std;


size_t SystemFrontend::tryRead(void* data, size_t maxlen) {
	lock_guard<mutex> lock(bufReadLock);

	if (bufRead.size() >= maxlen) {
		auto it = bufRead.begin();
		for (int i = 0; i < maxlen; i++) {
			((char*)data)[i] = *it;
			it++;
		}
		bufRead.erase(bufRead.begin(), it);
		return maxlen;
	} else if (bufRead.empty())
		return 0;
	else {
		auto it = bufRead.begin();
		size_t ans = bufRead.size();
		for (int i = 0; i < ans; i++) {
			((char*)data)[i] = *it;
			it++;
		}
		bufRead.clear();
		return ans;
	}
}


size_t SystemFrontend::getBufferedSize() {
	bufReadLock.lock();
	size_t ans = bufRead.size();
	bufReadLock.unlock();
	return ans;
}

