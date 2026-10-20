// Step 4 generator: analyze, validate and write; never launches guest code.
#include <rex/codegen/codegen.h>
#include <rex/logging.h>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  rex::InitLogging("analysis.log", spdlog::level::info);
  auto pipeline = rex::codegen::CodegenPipeline::Create(argv[1]);
  if (!pipeline) { std::cerr << pipeline.error().message << '\n'; return 3; }
  auto& ctx = pipeline->context();
  auto& binary = ctx.binary();
  std::ofstream metadata("loader.tsv");
  metadata << "base\t" << binary.baseAddress() << "\nsize\t" << binary.imageSize()
           << "\nentry\t" << binary.entryPoint() << '\n';
  bool bounds = true;
  for (auto& section : binary.sections()) {
    metadata << "section\t" << section.name << '\t' << section.baseAddress << '\t'
             << section.size << '\t' << section.executable << '\n';
    if (section.baseAddress < binary.baseAddress() ||
        uint64_t(section.baseAddress) + section.size >
        uint64_t(binary.baseAddress()) + binary.imageSize()) bounds = false;
  }
  for (auto& symbol : binary.importSymbols())
    metadata << "import\t" << symbol.address << '\t' << symbol.name << '\n';
  metadata.close();
  if (!bounds || binary.baseAddress() != 0x82000000 ||
      binary.entryPoint() != 0x822e6738 || binary.imageSize() != 10289152) {
    std::cerr << "Loader metadata/bounds gate failed; analysis not started\n";
    return 4;
  }
  auto result = pipeline->RunAnalyze();
  std::ofstream discovery("discovery.tsv");
  discovery << "analysis_success\t" << bool(result) << "\npdata_count\t"
            << ctx.scan.pdataSizes.size() << "\nerror_count\t" << ctx.errors.Count() << '\n';
  for (auto& [address, size] : ctx.scan.pdataSizes)
    discovery << "pdata\t" << address << '\t' << size << '\n';
  for (auto& [address, node] : ctx.graph.functions())
    discovery << "function\t" << address << '\t' << node->size() << '\t'
              << int(node->authority()) << '\t' << node->name() << '\n';
  for (auto& error : ctx.errors.Entries())
    discovery << "error\t" << ctx.errors.CategoryName(error.category) << '\t'
              << error.address << '\t' << error.secondaryAddress << '\t' << error.message << '\n';
  std::ofstream detail("graph-detail.tsv");
  for (auto& [address, node] : ctx.graph.functions()) {
    detail << "node\t" << address << '\t' << int(node->state()) << '\t' << node->instructions().size() << '\t' << node->unresolvedJumps().size() << '\n';
    for (auto& block : node->blocks()) detail << "block\t" << address << '\t' << block.base << '\t' << block.size << '\n';
    for (auto& edge : node->calls()) detail << "call\t" << address << '\t' << edge.site << '\t' << edge.target.isResolved() << '\n';
    for (auto& edge : node->tailCalls()) detail << "tail\t" << address << '\t' << edge.site << '\t' << edge.target.isResolved() << '\n';
    for (auto& table : node->jumpTables()) {
      detail << "table\t" << address << '\t' << table.bctrAddress << '\t' << table.targets.size() << '\n';
      for (auto target : table.targets) detail << "target\t" << address << '\t' << table.bctrAddress << '\t' << target << '\n';
    }
  }
  if (!result) std::cerr << result.error().message << '\n';
  if (!result || ctx.errors.HasErrors()) return 5;
  auto written = pipeline->RunWrite(false);
  if (!written) std::cerr << written.error().message << '\n';
  return written ? 0 : 6;
}
