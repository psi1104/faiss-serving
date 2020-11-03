#include <memory>
#include <sstream>
#include <string>

#include "faiss/Index.h"
#include "faiss/IndexHNSW.h"
#include "faiss/MetaIndexes.h"
#include "faiss/index_io.h"
#include "httplib.h"
#include "spdlog/spdlog.h"

#include "mapping.hh"
#include "utils.hh"

int main(int argc, char** argv) {
  auto config = fs::parseCLIArgs(argc, argv);

  spdlog::info("Configuration");
  spdlog::info(" - Host: {}", config.host);
  spdlog::info(" - Port: {}", config.port);
  spdlog::info(" - Num of Http Listener Threads: {}", config.listenerThreads);
  spdlog::info(" - IndexFile: {}", config.indexFile);
  spdlog::info(" - Default numK: {}", config.numK);
  spdlog::info(" - EfSearch Value: {}", config.hnswEfSearch);
  spdlog::info(" - Map File: {}", config.mapFile);

  httplib::Server server;
  server.new_task_queue = [config] { return new httplib::ThreadPool(config.listenerThreads); };

  spdlog::info("Start loading {}", config.indexFile);
  faiss::Index* index = faiss::read_index(config.indexFile.c_str(), faiss::IO_FLAG_READ_ONLY);
  spdlog::info("ntotal: {}", index->ntotal);

  if (config.hnswEfSearch != 0) {
    auto indexHNSW = dynamic_cast<faiss::IndexHNSWPQ*>(dynamic_cast<faiss::IndexIDMap*>(index)->index);
    indexHNSW->hnsw.efSearch = config.hnswEfSearch;
    spdlog::info("Set efsearch as {}", config.hnswEfSearch);
  }

  fs::Mapper mapper;
  if (!config.mapFile.empty()) {
    spdlog::info("Loading map-file: {}", config.mapFile);
    mapper.open(config.mapFile);
    spdlog::info("Loaded {} rows", mapper.getNumRows());

    if (index->ntotal > mapper.getNumRows())
      throw std::runtime_error("index->ntotal > mapper.getNumRows()");
  }

  server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
    spdlog::info("{} {} HTTP/{} {} - from {}", req.method, req.path, req.version, res.status, req.remote_addr);
  });

  server.Post("/v1/search", [index, config, &mapper](const httplib::Request& req, httplib::Response& res) {
    try {
      std::vector<float> queryVector;
      int64_t numK = -1;
      size_t numQueries = fs::parseJsonPayload(req.body, index->d, queryVector, numK);
      if (numK <= 0)
        numK = config.numK;

      std::vector<float> distances(numK * numQueries);
      std::vector<faiss::Index::idx_t> labels(numK * numQueries);

      index->search(numQueries, queryVector.data(), (faiss::Index::idx_t)(numK == -1 ? config.numK : numK),
                    distances.data(), labels.data());

      if (mapper.isOpened()) {
        std::vector<std::string> values;
        values.reserve(labels.size());
        for (int i = 0; i < labels.size(); i++)
          values.push_back(mapper.getItem(labels.at(i)));

        res.set_content(fs::constructJson(labels, distances, numK, numQueries, &values), "application/json");
      } else {
        res.set_content(fs::constructJson(labels, distances, numK, numQueries), "application/json");
      }
    } catch (std::exception& error) {
      spdlog::error(error.what());

      res.status = 400;
      res.set_content("{\"reason\": \"" + std::string(error.what()) + "\"}", "application/json");
    }
  });

  spdlog::info("Running server on {}:{}", config.host, config.port);
  server.listen(config.host.c_str(), config.port);

  return 0;
}
