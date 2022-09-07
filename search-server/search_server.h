#pragma once

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <execution>


const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(std::string_view stop_words_text);
	explicit SearchServer(const std::string& stop_words_text);

	void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

	template <typename DocumentPredicate, typename Policy>
	std::vector<Document> FindTopDocuments(Policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
	
	template <typename Policy>
	std::vector<Document> FindTopDocuments(Policy policy, std::string_view raw_query, DocumentStatus status) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

	template <typename Policy>
	std::vector<Document> FindTopDocuments(Policy policy, std::string_view raw_query) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

	int GetDocumentCount() const;

	std::set<int>::const_iterator begin() const;
	std::set<int>::const_iterator end() const;

	template <typename Policy>
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(Policy policy, std::string_view raw_query, int document_id) const;
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	template <typename Policy>
	void RemoveDocument(Policy policy, int document_id);
	void RemoveDocument(int document_id);

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const std::set<std::string, std::less<>> stop_words_;

	std::set<std::string> words_;
	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
	
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;

	bool IsStopWord(std::string_view word) const;

	static bool IsValidWord(std::string_view word);

	std::vector<std::string_view> SplitIntoWordsNoStopAndAddWords(std::string_view text);

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query {
		std::set<std::string_view> plus_words;
		std::set<std::string_view> minus_words;
	};

	template<typename Policy>
	SearchServer::Query ParseQuery(Policy policy, std::string_view text) const;

	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	template <typename DocumentPredicate, typename Policy>
	std::vector<Document> FindAllDocuments(ConcurrentMap<int, double>& document_to_relevance, Policy policy,
											const SearchServer::Query& query, 
											DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::sequenced_policy policy, const Query& query, DocumentPredicate document_predicate) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::parallel_policy policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
	if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		using namespace std::literals::string_literals;
		throw std::invalid_argument("Some of stop words are invalid"s);
	}
}

template <typename DocumentPredicate, typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(Policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(policy, raw_query);

	auto matched_documents = FindAllDocuments(policy, query, document_predicate);

	sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
			return lhs.rating > rhs.rating;
		}
		else {
			return lhs.relevance > rhs.relevance;
		}
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}

	return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
	return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(Policy policy, std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		}
	);
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(Policy policy, std::string_view raw_query) const {
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename Policy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(Policy policy, std::string_view raw_query, 
																					  int document_id) const {
	if (document_ids_.find(document_id) == document_ids_.end()) {
		using namespace std::literals::string_literals;
		throw std::invalid_argument("Invalid document_id"s);
	}

	const auto query = ParseQuery(policy, raw_query);

	std::atomic_int count = 0;
	std::atomic_bool no_minus_word = true;

	std::vector<std::pair<std::string_view, double>> matched_words_freqs(query.plus_words.size());

	copy_if(policy, document_to_word_freqs_.at(document_id).begin(), document_to_word_freqs_.at(document_id).end(), 
			matched_words_freqs.begin(), [&query, &no_minus_word, &count](std::pair<std::string_view, double> word_freqs) {
				if (no_minus_word) {
					no_minus_word = !query.minus_words.count(word_freqs.first) && no_minus_word;
					return query.plus_words.count(word_freqs.first) ? static_cast<bool>(++count) : false;
				} 
				else {
					return false;
				}				
			}
	);
	if (!no_minus_word) {
		count = 0;
	}
		
	matched_words_freqs.resize(count);
	std::vector<std::string_view> matched_words(count);

	transform(policy, matched_words_freqs.begin(), matched_words_freqs.end(), matched_words.begin(), 
				[](std::pair<std::string_view, double> word_freqs) { 
					return word_freqs.first;
				}
	);

	return { matched_words, documents_.at(document_id).status };
}

template <typename Policy>
void SearchServer::RemoveDocument(Policy policy, int document_id) {
	auto document_ids_it = document_ids_.find(document_id);

	if (document_ids_it == document_ids_.end()) {
		using namespace std::literals::string_literals;
		throw std::invalid_argument("Invalid document_id"s);
	}

	auto document_words = move(document_to_word_freqs_[document_id]);
	for_each(policy, document_words.begin(), document_words.end(), 
			[this, document_id](const auto& word) {
				word_to_document_freqs_[word.first].erase(document_id); 
			}
	);

	document_to_word_freqs_.erase(document_id);
	documents_.erase(document_id);
	document_ids_.erase(document_ids_it);
}

template<typename Policy>
SearchServer::Query SearchServer::ParseQuery(Policy policy, std::string_view text) const {

	auto words = SplitIntoWords(text);
	std::vector<QueryWord> query_words(words.size());
	std::transform(policy, words.begin(), words.end(), query_words.begin(), [this](std::string_view word) {return ParseQueryWord(word); });

	Query result;

	for (QueryWord& query_word : query_words) {
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.insert(query_word.data);
			}
			else {
				result.plus_words.insert(query_word.data);
			}
		}
	}
	return result;
}

template <typename DocumentPredicate, typename Policy>
std::vector<Document> SearchServer::FindAllDocuments(ConcurrentMap<int, double>& document_to_relevance, Policy policy,
	const SearchServer::Query& query,
	DocumentPredicate document_predicate) const {

	std::for_each(policy,
		query.plus_words.begin(),
		query.plus_words.end(),
		[this, &document_predicate, &document_to_relevance](std::string_view word) {
			if (word_to_document_freqs_.find(word) != word_to_document_freqs_.end()) {
				const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
				for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
					const auto& document_data = documents_.at(document_id);
					if (document_predicate(document_id, document_data.status, document_data.rating)) {
						document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
					}
				}
			}
		}
	);

	for (std::string_view word : query.minus_words) {
		if (word_to_document_freqs_.find(word) == word_to_document_freqs_.end()) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (auto& [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const {


	ConcurrentMap<int, double> document_to_relevance(1);
	return FindAllDocuments(document_to_relevance, policy, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const {

	ConcurrentMap<int, double> document_to_relevance(240);
	return FindAllDocuments(document_to_relevance, policy, query, document_predicate);
}