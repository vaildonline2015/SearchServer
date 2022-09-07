#include "search_server.h"
#include "process_queries.h"
#include "log_duration.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <string_view>
#include <execution>
#include <iostream>
#include <random>

using namespace std;

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
	const string& func, unsigned line, const string& hint) {
	if (t != u) {
		cout << boolalpha;
		cout << file << "("s << line << "): "s << func << ": "s;
		cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cout << t << " != "s << u << "."s;
		if (!hint.empty()) {
			cout << " Hint: "s << hint;
		}
		cout << endl;
		abort();
	}
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
	const string& hint) {
	if (!value) {
		cout << file << "("s << line << "): "s << func << ": "s;
		cout << "ASSERT("s << expr_str << ") failed."s;
		if (!hint.empty()) {
			cout << " Hint: "s << hint;
		}
		cout << endl;
		abort();
	}
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template <typename T>
void RunTestImpl(T func, const string& func_name) {
	func();
	cerr << func_name << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), (#func))


void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server(""sv);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server("in the"sv);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
	}
}

void TestExcludeMinusWordsFromFoundResult() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	SearchServer server(""sv);
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	ASSERT_HINT(server.FindTopDocuments("-in"s).empty(), "Minus words must be excluded from result"s);
}

void TestMatchDocument() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	// Without minus
	{
		SearchServer server(""sv);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto [found_words, status] = server.MatchDocument("my cat loves the city"s, 42);
		ASSERT_EQUAL(found_words.size(), 3);
	}

	// With minus
	{
		SearchServer server(""sv);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto [found_words, status] = server.MatchDocument("my cat loves -the city"s, 42);
		ASSERT_HINT(found_words.empty(), "Documents with minus words must be excluded from result"s);
	}
}

void TestSortByRelevanceInFoundResult() {
	SearchServer server(""sv);
	server.AddDocument(0, "white cat and a fashionable collar"sv, DocumentStatus::ACTUAL, { 1, 2, 3 });
	server.AddDocument(1, "fluffy cat fluffy tail"sv, DocumentStatus::ACTUAL, { 1, 2, 3 });
	server.AddDocument(2, "groomed dog expressive eyes"sv, DocumentStatus::ACTUAL, { 1, 2, 3 });

	const auto found_docs = server.FindTopDocuments("fluffy groomed cat"sv);

	for (size_t i = 0; i + 1 < found_docs.size(); ++i) {
		ASSERT_HINT(found_docs[i].relevance > found_docs[i + 1].relevance,
			"Found documents must be sorted by decrease"s);
	}
}

void TestCalculateDocumentRating() {
	SearchServer server(""sv);
	server.AddDocument(0, "cat in the city"sv, DocumentStatus::ACTUAL, { 1, 3, 2 });
	const auto documents = server.FindTopDocuments("cat"sv);
	ASSERT_EQUAL(documents[0].rating, 2);
}

void TestFindTopDocumentsWithPredicate() {
	SearchServer server(""sv);
	server.AddDocument(0, "cat in the city"sv, DocumentStatus::ACTUAL, { 1, 3, 2 });
	server.AddDocument(1, "cat in the city"sv, DocumentStatus::ACTUAL, { 1, 3, 2 });

	const auto found_docs = server.FindTopDocuments("in"sv,
		[](int document_id, DocumentStatus status, int rating) { return document_id == 1; });
	ASSERT_EQUAL(found_docs.size(), 1);
}

void TestFindTopDocumentsWithStatus() {
	SearchServer server(""sv);
	server.AddDocument(0, "cat in the city"sv, DocumentStatus::ACTUAL, { 1, 3, 2 });
	server.AddDocument(1, "cat in the city"sv, DocumentStatus::BANNED, { 1, 3, 2 });

	const auto found_docs = server.FindTopDocuments("in"sv, DocumentStatus::BANNED);
	ASSERT_EQUAL_HINT(found_docs.size(), 1, "Found documents must be certain status"s);
}

bool Equal(double a, double b) {
	const double epsilon = 1e-6;

	if (a >= 1) {
		return abs(a - b) < epsilon;
	}
	else {
		return abs(a - b) < epsilon * abs(a);
	}
}

void TestCalculateDocumentRelevance() {
	SearchServer server(""sv);
	server.AddDocument(0, "white cat and a fashionable collar"sv, DocumentStatus::ACTUAL, { 1, 2, 3 });
	server.AddDocument(1, "fluffy cat fluffy tail"sv, DocumentStatus::ACTUAL, { 1, 2, 3 });
	server.AddDocument(2, "groomed dog expressive eyes"sv, DocumentStatus::ACTUAL, { 1, 2, 3 });

	const auto found_docs = server.FindTopDocuments("fluffy groomed cat"sv);

	ASSERT(Equal(found_docs[0].relevance, 0.65067242136109593));
}

void SplitIntoWordsTest() {
	

	vector<string_view> str1 = SplitIntoWords("");
	vector<string_view> str2 = SplitIntoWords(" ");
	vector<string_view> str3 = SplitIntoWords("  ");
	vector<string_view> str4 = SplitIntoWords(" a  ");
	vector<string_view> str5 = SplitIntoWords("asd asd");
	vector<string_view> str6 = SplitIntoWords(" asd   asd");
	vector<string_view> str7 = SplitIntoWords(" asd   asd   ");
	vector<string_view> str8 = SplitIntoWords("a");

	vector<string_view> str9 = { "asd"sv, "asd"sv };
	vector<string_view> str10 = { "a"sv };

	ASSERT_HINT(str1.empty(), "");
	ASSERT_HINT(str2.empty(), "");
	ASSERT_HINT(str3.empty(), "");
	ASSERT_HINT(str4 == str8, "");
	ASSERT_HINT(str4 == str10, "");
	ASSERT_HINT(str5 == str9, "");
	ASSERT_HINT(str5 == str6, "");
	ASSERT_HINT(str5 == str7, "");
}

void TestSearchServer() {
	RUN_TEST(SplitIntoWordsTest);
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestExcludeMinusWordsFromFoundResult);
	RUN_TEST(TestMatchDocument);
	RUN_TEST(TestSortByRelevanceInFoundResult);
	RUN_TEST(TestCalculateDocumentRating);
	RUN_TEST(TestFindTopDocumentsWithPredicate);
	RUN_TEST(TestFindTopDocumentsWithStatus);
	RUN_TEST(TestCalculateDocumentRelevance);

}

void SomeTest() {
	cout << "Search server testing finished"s << endl;

	SearchServer search_server("and with"s);

	int id = 0;
	for (
		const string& text : {
			"funny pet and nasty rat"s,
			"funny pet with curly hair"s,
			"funny pet and not very nasty rat"s,
			"pet with rat and rat and rat"s,
			"nasty rat with curly hair"s,
		}
		) {
		search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
	}

	const vector<string> queries = {
		"nasty rat -not"s,
		"not very funny nasty pet"s,
		"curly hair"s
	};
	for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
		cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
	}
}

void TaskTest() {
	SearchServer search_server("and with"s);

	int id = 0;
	for (
		const string& text : {
			"white cat and yellow hat"s,
			"curly cat curly tail"s,
			"nasty dog with big eyes"s,
			"nasty pigeon john"s,
		}
		) {
		search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
	}


	cout << "ACTUAL by default:"s << endl;

	for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
		PrintDocument(document);
	}
	cout << "BANNED:"s << endl;

	for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, DocumentStatus::BANNED)) {
		PrintDocument(document);
	}

	cout << "Even ids:"s << endl;

	for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
		PrintDocument(document);
	}
}



string GenerateWord(mt19937& generator, int max_length) {
	const int length = uniform_int_distribution(1, max_length)(generator);
	string word;
	word.reserve(length);
	for (int i = 0; i < length; ++i) {
		word.push_back(uniform_int_distribution(int('a'), int('z'))(generator));
	}
	return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
	vector<string> words;
	words.reserve(word_count);
	for (int i = 0; i < word_count; ++i) {
		words.push_back(GenerateWord(generator, max_length));
	}
	words.erase(unique(words.begin(), words.end()), words.end());
	return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
	string query;
	for (int i = 0; i < word_count; ++i) {
		if (!query.empty()) {
			query.push_back(' ');
		}
		if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
			query.push_back('-');
		}
		query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
	}
	return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
	vector<string> queries;
	queries.reserve(query_count);
	for (int i = 0; i < query_count; ++i) {
		queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
	}
	return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy) {
	LogDuration a(string(mark), cout);
	double total_relevance = 0;
	for (const string_view query : queries) {
		for (const auto& document : search_server.FindTopDocuments(policy, query)) {
			total_relevance += document.relevance;
		}
	}
	cout << total_relevance << endl;
}

#define TEST(policy) Test(#policy, search_server, queries, execution::policy)


int main() {
	TestSearchServer();
	SomeTest();
	TaskTest();

	mt19937 generator;

	const auto dictionary = GenerateDictionary(generator, 1000, 10);
	const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

	SearchServer search_server(dictionary[0]);
	for (size_t i = 0; i < documents.size(); ++i) {
		search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
	}

	const auto queries = GenerateQueries(generator, dictionary, 100, 70);

	TEST(seq);
	TEST(par);
}