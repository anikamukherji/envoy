#include "common/stats/isolated_store_impl.h"

#include <string.h>

#include <algorithm>
#include <string>

#include "common/common/utility.h"
#include "common/stats/fake_symbol_table_impl.h"
#include "common/stats/histogram_impl.h"
#include "common/stats/utility.h"

namespace Envoy {
namespace Stats {

IsolatedStoreImpl::IsolatedStoreImpl()
    : IsolatedStoreImpl(std::make_unique<FakeSymbolTableImpl>()) {}

IsolatedStoreImpl::IsolatedStoreImpl(std::unique_ptr<SymbolTable>&& symbol_table)
    : IsolatedStoreImpl(*symbol_table) {
  symbol_table_storage_ = std::move(symbol_table);
}

IsolatedStoreImpl::IsolatedStoreImpl(SymbolTable& symbol_table)
    : symbol_table_(symbol_table), alloc_(symbol_table_),
      counters_([this](const std::string& name) -> CounterSharedPtr {
        std::string tag_extracted_name = name;
        std::vector<Tag> tags;
        return alloc_.makeCounter(name, std::move(tag_extracted_name), std::move(tags));
      }),
      gauges_([this](const std::string& name) -> GaugeSharedPtr {
        std::string tag_extracted_name = name;
        std::vector<Tag> tags;
        return alloc_.makeGauge(name, std::move(tag_extracted_name), std::move(tags));
      }),
      histograms_([this](const std::string& name) -> HistogramSharedPtr {
        return std::make_shared<HistogramImpl>(name, *this, std::string(name), std::vector<Tag>());
      }) {}

struct IsolatedScopeImpl : public Scope {
  IsolatedScopeImpl(IsolatedStoreImpl& parent, const std::string& prefix)
      : parent_(parent), prefix_(Utility::sanitizeStatsName(prefix)) {}

  // Stats::Scope
  ScopePtr createScope(const std::string& name) override {
    return ScopePtr{new IsolatedScopeImpl(parent_, prefix_ + name)};
  }
  void deliverHistogramToSinks(const Histogram&, uint64_t) override {}
  Counter& counter(const std::string& name) override { return parent_.counter(prefix_ + name); }
  Gauge& gauge(const std::string& name) override { return parent_.gauge(prefix_ + name); }
  NullGaugeImpl& nullGauge(const std::string&) override { return null_gauge_; }
  Histogram& histogram(const std::string& name) override {
    return parent_.histogram(prefix_ + name);
  }
  const Stats::StatsOptions& statsOptions() const override { return parent_.statsOptions(); }
  const SymbolTable& symbolTable() const override { return parent_.symbolTable(); }
  SymbolTable& symbolTable() override { return parent_.symbolTable(); }

  IsolatedStoreImpl& parent_;
  NullGaugeImpl null_gauge_;
  const std::string prefix_;
};

ScopePtr IsolatedStoreImpl::createScope(const std::string& name) {
  return ScopePtr{new IsolatedScopeImpl(*this, name)};
}

} // namespace Stats
} // namespace Envoy
