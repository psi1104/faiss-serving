#pragma once

#include <sstream>
#include <string>
#include <type_traits>
#include "cxxopts.hpp"
#include "faiss/Index.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace fs {

struct CLIArguments {
  const std::string host;
  const std::string indexFile;
  const size_t port;
  const size_t listenerThreads;
  const size_t numK;
  const size_t hnswEfSearch;
  const std::string mapFile;
};

inline const CLIArguments parseCLIArgs(int argc, char** argv) {
  cxxopts::Options options(argv[0], "A lightweight Faiss HTTP Server\n");

  // clang-format off
  options.add_options()
    ("host", "Host", cxxopts::value<std::string>()->default_value("localhost"))
    ("p,port", "Port", cxxopts::value<size_t>()->default_value("8080"))
    ("i,index-file", "Faiss index file path", cxxopts::value<std::string>()->default_value(""))
    ("t,listener-threads", "Num threads to use for http server", cxxopts::value<size_t>()->default_value("4"))
    ("k,num-k", "Default num k", cxxopts::value<size_t>()->default_value("800"))
    ("hnsw-ef-search", "efSearch Value for hnsw index", cxxopts::value<size_t>()->default_value("0"))
    ("m,map-file", "Use map file to return real values instead of indices", cxxopts::value<std::string>()->default_value(""))
    ("h,help", "Print usage");
  // clang-format on

  auto args = options.parse(argc, argv);

  if (args.count("help")) {
    std::cout << options.help() << std::endl;
    std::exit(0);
  }

  return {args["host"].as<std::string>(),    args["index-file"].as<std::string>(),
          args["port"].as<size_t>(),         args["listener-threads"].as<size_t>(),
          args["num-k"].as<size_t>(),        args["hnsw-ef-search"].as<size_t>(),
          args["map-file"].as<std::string>()};
}

inline size_t parseJsonPayload(const std::string& payload,
                               size_t dimension,
                               std::vector<float>& outputVector,
                               int64_t& numK) {
  rapidjson::Document document;
  document.Parse(payload.c_str());

  if (!document.IsObject())
    throw std::runtime_error("Cannot parse json object. Root object must be json object.");

  if (!document.HasMember("queries") || !document["queries"].IsArray())
    throw std::runtime_error("Json object does not have 'queries' or 'queries' is not a json array.");

  auto queries = document["queries"].GetArray();
  if (document.HasMember("top_k") && document["top_k"].IsInt())
    numK = document["top_k"].GetInt();

  outputVector.reserve(queries.Size() * dimension);

  for (const auto& query : queries) {
    if (!query.IsArray())
      throw std::runtime_error("queries should be float 2d array");

    auto queryArray = query.GetArray();

    if (queryArray.Size() != dimension)
      throw std::runtime_error("Dimension mismatch.");

    for (const auto& item : queryArray) {
      if (!item.IsNumber())
        throw std::runtime_error("queries should be float 2d array");
      outputVector.push_back(item.GetFloat());
    }
  }

  return queries.Size();
}

template <typename T>
inline std::string constructJson(const std::vector<T>& labels,
                                 const std::vector<float>& distances,
                                 int64_t numK,
                                 size_t numQueries,
                                 const std::vector<std::string>* strings = nullptr) {
  rapidjson::Document document;
  document.SetObject();

  rapidjson::Value distanceArrays(rapidjson::kArrayType);
  rapidjson::Value labelArrays(rapidjson::kArrayType);
  rapidjson::Value stringArrays(rapidjson::kArrayType);

  for (size_t i = 0; i < numQueries; i++) {
    rapidjson::Value distanceArray(rapidjson::kArrayType);
    rapidjson::Value labelArray(rapidjson::kArrayType);
    rapidjson::Value stringArray(rapidjson::kArrayType);

    for (size_t j = 0; j < numK; j++) {
      distanceArray.PushBack(rapidjson::Value(distances[i * numK + j]).Move(), document.GetAllocator());
      labelArray.PushBack(rapidjson::Value(labels[i * numK + j]).Move(), document.GetAllocator());

      if (strings != nullptr)
        stringArray.PushBack(
            rapidjson::Value(rapidjson::StringRef((*strings)[i * numK + j].c_str()), document.GetAllocator()).Move(),
            document.GetAllocator());
    }

    distanceArrays.PushBack(distanceArray, document.GetAllocator());
    labelArrays.PushBack(labelArray, document.GetAllocator());

    if (strings != nullptr)
      stringArrays.PushBack(stringArray, document.GetAllocator());
  }

  document.AddMember("distances", distanceArrays, document.GetAllocator());
  document.AddMember("indices", labelArrays, document.GetAllocator());

  if (strings != nullptr)
    document.AddMember("strings", stringArrays, document.GetAllocator());

  rapidjson::StringBuffer buffer;
  buffer.Clear();
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);
  return std::string(buffer.GetString());
}

}  // namespace fs
