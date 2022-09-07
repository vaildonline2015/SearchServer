#pragma once

#include "search_server.h"
#include "basic_iterator.h"

#include <vector>
#include <string>
#include <list>

class  Docs {

	using BasicIterator = BasicIterator<Document, std::vector<Document>::iterator, std::vector<std::vector<Document>>::iterator>;

public:

	Docs(const Docs& rhs) : docs_(rhs.docs_) {
	}

	Docs(Docs&& rhs) noexcept : docs_(move(rhs.docs_)) {
	}

	Docs(const std::vector<std::vector<Document>>& docs) : docs_(docs) {
	}

	Docs(std::vector<std::vector<Document>>&& docs) : docs_(move(docs)) {
	}

	BasicIterator begin() {
		return BasicIterator(docs_.begin(), docs_.end());
	}

	BasicIterator end() {
		return BasicIterator(docs_.end(), docs_.end());
	}

private:
	std::vector<std::vector<Document>> docs_;
};

Docs ProcessQueriesJoined(
	const SearchServer& search_server,
	const std::vector<std::string>& queries);

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
	const std::vector<std::string>& queries);