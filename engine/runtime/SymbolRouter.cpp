#include "engine/runtime/SymbolRouter.hpp"

namespace trading {

bool SymbolRouter::register_symbol(const SymbolConfig& cfg,
                                    std::unique_ptr<Strategy> strategy) {
    if (cfg.symbol_id >= kMaxSymbols) return false;
    if (slots_[cfg.symbol_id]) return false;  // already registered

    slots_[cfg.symbol_id] =
        std::make_unique<SymbolPipeline>(cfg.symbol_id, std::move(strategy));
    ++count_;
    return true;
}

MatchOutput SymbolRouter::process(const Event& event) {
    const uint32_t sid = event.symbol_id;
    if (sid >= kMaxSymbols || !slots_[sid]) return {};

    auto& pipe = *slots_[sid];
    const auto sig = pipe.strategy->process(event);
    if (sig == StrategySignal::Noop) return {};

    const auto etype = event_type(event);
    if (etype == EventType::NewOrder)
        return pipe.matcher.process_new_order(*pipe.book, event);
    if (etype == EventType::CancelOrder) {
        pipe.matcher.process_cancel(*pipe.book, event);
        return {};
    }
    if (etype == EventType::ModifyOrder) {
        pipe.matcher.process_modify(*pipe.book, event);
        return {};
    }
    return {};
}

OrderBook* SymbolRouter::book(uint32_t symbol_id) noexcept {
    if (symbol_id >= kMaxSymbols || !slots_[symbol_id]) return nullptr;
    return slots_[symbol_id]->book.get();
}

StrategyEngine* SymbolRouter::strategy(uint32_t symbol_id) noexcept {
    if (symbol_id >= kMaxSymbols || !slots_[symbol_id]) return nullptr;
    return slots_[symbol_id]->strategy.get();
}

} // namespace trading
