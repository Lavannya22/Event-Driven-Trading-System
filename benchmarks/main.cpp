// Benchmark runner CLI.
// Usage: run_benchmark [dataset] [--baseline] [--json <out>] [--csv <out>]
//
// Examples:
//   run_benchmark                          # runs small dataset
//   run_benchmark medium                   # runs medium dataset
//   run_benchmark small --baseline         # compare against stored baseline
//   run_benchmark small --json out.json    # save JSON report

#include "benchmarks/BenchmarkRunner.hpp"
#include <cstdio>
#include <filesystem>
#include <string>

#ifndef BENCHMARK_DATASETS_DIR
#define BENCHMARK_DATASETS_DIR "benchmarks/datasets"
#endif
#ifndef BENCHMARK_BASELINES_DIR
#define BENCHMARK_BASELINES_DIR "benchmarks/baselines"
#endif
#ifndef BENCHMARK_REPORTS_DIR
#define BENCHMARK_REPORTS_DIR "benchmarks/reports"
#endif

int main(int argc, char* argv[]) {
    using namespace trading::bench;

    std::string dataset   = "small";
    bool   check_baseline = false;
    std::string json_out;
    std::string csv_out;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--baseline")  check_baseline = true;
        else if (arg == "--json" && i + 1 < argc) json_out = argv[++i];
        else if (arg == "--csv"  && i + 1 < argc) csv_out  = argv[++i];
        else dataset = arg;
    }

    const std::string datasets_dir  = BENCHMARK_DATASETS_DIR;
    const std::string baselines_dir = BENCHMARK_BASELINES_DIR;
    const std::string reports_dir   = BENCHMARK_REPORTS_DIR;

    const bool is_binary = (dataset == "large");
    const std::string ext  = is_binary ? ".bin" : ".csv";
    const std::string path = datasets_dir + "/" + dataset + ext;

    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr,
            "Dataset not found: %s\n"
            "Run gen_datasets first: ./gen_datasets %s\n",
            path.c_str(), datasets_dir.c_str());
        return 1;
    }

    BenchmarkConfig cfg;
    cfg.name           = dataset;
    cfg.dataset_path   = path;
    cfg.binary_dataset = is_binary;

    BenchmarkRunner runner;
    auto report = runner.run(cfg);
    runner.print_report(report);

    // Save reports
    std::filesystem::create_directories(reports_dir);
    const std::string default_json = reports_dir + "/" + dataset + "_latest.json";
    const std::string default_csv  = reports_dir + "/" + dataset + "_history.csv";

    runner.save_json(report, json_out.empty() ? default_json : json_out);
    runner.save_csv(report,  csv_out.empty()  ? default_csv  : csv_out);

    // Baseline comparison
    if (check_baseline) {
        const std::string baseline = baselines_dir + "/baseline_" + dataset + ".json";
        const bool passed = runner.compare_baseline(report, baseline);
        return passed ? 0 : 1;
    }

    return 0;
}
