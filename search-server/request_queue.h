#pragma once

#include <vector>
#include <deque>

#include "search_server.h"
#include "document.h"

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer& search_server);

	template <typename DocumentPredicate>
	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

	std::vector<Document> AddFindRequest(const std::string& raw_query);

	int GetNoResultRequests() const;
private:
	struct QueryResult {
		QueryResult(bool empty) : empty_(empty) {}
		bool empty_;
	};
	std::deque<QueryResult> requests_;
	const static int sec_in_day_ = 1440;
	const SearchServer& search_server_;
	size_t no_result_request_count_ = 0;

	void AddFindRequest(const std::vector<Document>& found_documents);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
	std::vector<Document> found_documents = search_server_.FindTopDocuments(raw_query, document_predicate);
	AddFindRequest(found_documents);
	return found_documents;
}