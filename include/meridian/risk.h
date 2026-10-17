#ifndef MERIDIAN_RISK_H
#define MERIDIAN_RISK_H

#include <stddef.h>
#include <stdint.h>

#include "meridian/amount.h"
#include "meridian/digest.h"
#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD_RISK_MAX_POSITIONS 16u
#define MD_RISK_LABEL_LENGTH 31u

typedef enum MdRiskDecision {
    MD_RISK_APPROVE = 0,
    MD_RISK_REVIEW = 1,
    MD_RISK_HOLD = 2
} MdRiskDecision;

typedef struct MdRiskPolicy {
    uint32_t stress_default_bps;
    uint32_t liquidity_haircut_bps;
    uint32_t concentration_threshold_bps;
    uint32_t concentration_charge_bps;
    uint32_t minimum_coverage_bps;
    uint32_t maximum_single_lane_bps;
    uint32_t maximum_hhi_bps;
} MdRiskPolicy;

typedef struct MdLaneRiskPosition {
    MdDigest lane_id;
    char label[MD_RISK_LABEL_LENGTH + 1u];
    MdAmount gross_volume;
    MdAmount pending_settlement;
    MdAmount available_liquidity;
    MdAmount fee_reserve;
    uint32_t default_probability_bps;
    uint32_t recovery_bps;
    int active;
} MdLaneRiskPosition;

typedef struct MdRiskPortfolio {
    MdLaneRiskPosition positions[MD_RISK_MAX_POSITIONS];
    size_t position_count;
} MdRiskPortfolio;

typedef struct MdRiskReport {
    MdAmount gross_volume;
    MdAmount pending_settlement;
    MdAmount available_liquidity;
    MdAmount expected_loss;
    MdAmount stress_loss;
    MdAmount loss_charge;
    MdAmount liquidity_gap;
    MdAmount fee_reserve;
    MdAmount concentration_addon;
    MdAmount required_capital;
    MdAmount target_capital;
    MdAmount available_capital;
    MdAmount capital_surplus;
    MdAmount capital_shortfall;
    uint32_t coverage_bps;
    uint32_t hhi_bps;
    uint32_t largest_lane_share_bps;
    size_t active_positions;
    MdRiskDecision decision;
    MdDigest digest;
} MdRiskReport;

MdRiskPolicy md_risk_policy_operating(void);
MdRiskPolicy md_risk_policy_severe(void);
MdStatus md_risk_policy_validate(const MdRiskPolicy *policy);
void md_risk_portfolio_init(MdRiskPortfolio *portfolio);
MdStatus md_risk_portfolio_add(MdRiskPortfolio *portfolio, MdLaneRiskPosition position);
MdStatus md_risk_analyze(const MdRiskPortfolio *portfolio, const MdRiskPolicy *policy, MdAmount available_capital, MdRiskReport *out);
const char *md_risk_decision_name(MdRiskDecision decision);

#ifdef __cplusplus
}
#endif

#endif
