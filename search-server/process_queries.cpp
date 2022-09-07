#include "search_server.h"
#include "process_queries.h"

#include <vector>
#include <string>
#include <list>
#include <execution>
#include <functional>

using namespace std;

Docs ProcessQueriesJoined(const SearchServer& search_server,
						  const std::vector<std::string>& queries) {
	return Docs(ProcessQueries(search_server, queries));
}

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
	const std::vector<std::string>& queries) {
	vector<vector<Document>> responses(queries.size());

	transform(execution::par, queries.begin(), queries.end(), responses.begin(),
		[&search_server](const string& query) {
			return search_server.FindTopDocuments(query);
		}
	);

	return responses;
}