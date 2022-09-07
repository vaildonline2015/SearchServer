#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
	vector<Document> found_documents = search_server_.FindTopDocuments(raw_query, status);
	AddFindRequest(found_documents);
	return found_documents;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
	vector<Document> found_documents = search_server_.FindTopDocuments(raw_query);
	AddFindRequest(found_documents);
	return found_documents;
}

int RequestQueue::GetNoResultRequests() const {
	return no_result_request_count_;
}

void RequestQueue::AddFindRequest(const vector<Document>& found_documents) {
	if (found_documents.empty()) {
		++no_result_request_count_;
		requests_.push_back(true);
	}
	else {
		requests_.push_back(false);
	}

	if (requests_.size() > sec_in_day_) {
		if (requests_.front().empty_) {
			--no_result_request_count_;
		}
		requests_.pop_front();
	}
}