#include "meridian/risk.h"

#include "meridian/codec.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static MdStatus md_risk_add(MdAmount *total, MdAmount value) {
    MdAmount next;
    MdStatus status;

    status = md_amount_add(*total, value, &next);
    if (status != MD_OK) {
        return status;
    }
    *total = next;
    return MD_OK;
}

static uint32_t md_ratio_scaled(MdAmount numerator, MdAmount denominator, uint32_t scale) {
    MdAmount remainder;
    uint32_t result;
    uint32_t index;

    if (denominator == 0u || numerator == 0u || scale == 0u) {
        return 0u;
    }
    if (numerator >= denominator) {
        return scale;
    }
    remainder = 0u;
    result = 0u;
    for (index = 0u; index < scale; index += 1u) {
        if (remainder >= denominator - numerator) {
            remainder -= denominator - numerator;
            result += 1u;
        } else {
            remainder += numerator;
        }
    }
    return result;
}

static uint32_t md_coverage_bps(MdAmount available, MdAmount required) {
    MdAmount quotient;
    MdAmount remainder;
    uint32_t fractional;

    if (required == 0u) {
        return available == 0u ? 0u : 30000u;
    }
    quotient = available / required;
    if (quotient >= 3u) {
        return 30000u;
    }
    remainder = available % required;
    fractional = md_ratio_scaled(remainder, required, MD_BPS_DENOMINATOR);
    return (uint32_t)(quotient * MD_BPS_DENOMINATOR) + fractional;
}

static MdStatus md_expected_loss(const MdLaneRiskPosition *position, MdAmount *out) {
    MdAmount probability_weighted;
    uint32_t loss_given_default;
    MdStatus status;

    loss_given_default = MD_BPS_DENOMINATOR - position->recovery_bps;
    status = md_amount_mul_bps(position->pending_settlement, position->default_probability_bps, &probability_weighted);
    if (status != MD_OK) {
        return status;
    }
    return md_amount_mul_bps(probability_weighted, loss_given_default, out);
}

static MdStatus md_liquidity_gap(const MdLaneRiskPosition *position, const MdRiskPolicy *policy, MdAmount *out) {
    MdAmount stressed_liquidity;
    MdStatus status;

    status = md_amount_mul_bps(position->available_liquidity, MD_BPS_DENOMINATOR - policy->liquidity_haircut_bps, &stressed_liquidity);
    if (status != MD_OK) {
        return status;
    }
    *out = position->pending_settlement > stressed_liquidity ? position->pending_settlement - stressed_liquidity : 0u;
    return MD_OK;
}

static MdStatus md_report_digest(MdRiskReport *report) {
    char payload[2048];
    MdWriter writer;

    md_writer_init(&writer, payload, sizeof(payload));
    md_writer_amount(&writer, "gross_volume", report->gross_volume);
    md_writer_amount(&writer, "pending_settlement", report->pending_settlement);
    md_writer_amount(&writer, "available_liquidity", report->available_liquidity);
    md_writer_amount(&writer, "expected_loss", report->expected_loss);
    md_writer_amount(&writer, "stress_loss", report->stress_loss);
    md_writer_amount(&writer, "loss_charge", report->loss_charge);
    md_writer_amount(&writer, "liquidity_gap", report->liquidity_gap);
    md_writer_amount(&writer, "fee_reserve", report->fee_reserve);
    md_writer_amount(&writer, "concentration_addon", report->concentration_addon);
    md_writer_amount(&writer, "required_capital", report->required_capital);
    md_writer_amount(&writer, "target_capital", report->target_capital);
    md_writer_amount(&writer, "available_capital", report->available_capital);
    md_writer_amount(&writer, "capital_surplus", report->capital_surplus);
    md_writer_amount(&writer, "capital_shortfall", report->capital_shortfall);
    md_writer_u32(&writer, "coverage_bps", report->coverage_bps);
    md_writer_u32(&writer, "hhi_bps", report->hhi_bps);
    md_writer_u32(&writer, "largest_lane_share_bps", report->largest_lane_share_bps);
    md_writer_u32(&writer, "active_positions", (uint32_t)report->active_positions);
    md_writer_u32(&writer, "decision", (uint32_t)report->decision);
    if (md_writer_status(&writer) != MD_OK) {
        return md_writer_status(&writer);
    }
    report->digest = md_digest_canonical("meridian-risk-report-v1", payload);
    return MD_OK;
}

MdRiskPolicy md_risk_policy_operating(void) {
    MdRiskPolicy policy;

    policy.stress_default_bps = 1500u;
    policy.liquidity_haircut_bps = 1200u;
    policy.concentration_threshold_bps = 2500u;
    policy.concentration_charge_bps = 1800u;
    policy.minimum_coverage_bps = 12500u;
    policy.maximum_single_lane_bps = 5500u;
    policy.maximum_hhi_bps = 4200u;
    return policy;
}

MdRiskPolicy md_risk_policy_severe(void) {
    MdRiskPolicy policy;

    policy.stress_default_bps = 4200u;
    policy.liquidity_haircut_bps = 3500u;
    policy.concentration_threshold_bps = 2000u;
    policy.concentration_charge_bps = 3200u;
    policy.minimum_coverage_bps = 16000u;
    policy.maximum_single_lane_bps = 4000u;
    policy.maximum_hhi_bps = 3000u;
    return policy;
}

MdStatus md_risk_policy_validate(const MdRiskPolicy *policy) {
    if (policy == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (policy->stress_default_bps > MD_BPS_DENOMINATOR ||
        policy->liquidity_haircut_bps > MD_BPS_DENOMINATOR ||
        policy->concentration_threshold_bps > MD_BPS_DENOMINATOR ||
        policy->concentration_charge_bps > MD_BPS_DENOMINATOR ||
        policy->maximum_single_lane_bps == 0u ||
        policy->maximum_single_lane_bps > MD_BPS_DENOMINATOR ||
        policy->maximum_hhi_bps == 0u ||
        policy->maximum_hhi_bps > MD_BPS_DENOMINATOR ||
        policy->minimum_coverage_bps < MD_BPS_DENOMINATOR ||
        policy->minimum_coverage_bps > 30000u) {
        return MD_ERR_POLICY;
    }
    return MD_OK;
}

void md_risk_portfolio_init(MdRiskPortfolio *portfolio) {
    if (portfolio != 0) {
        memset(portfolio, 0, sizeof(*portfolio));
    }
}

MdStatus md_risk_portfolio_add(MdRiskPortfolio *portfolio, MdLaneRiskPosition position) {
    size_t index;

    if (portfolio == 0 || position.label[0] == '\0' || position.recovery_bps > MD_BPS_DENOMINATOR ||
        position.default_probability_bps > MD_BPS_DENOMINATOR) {
        return MD_ERR_ARGUMENT;
    }
    if (portfolio->position_count >= MD_RISK_MAX_POSITIONS) {
        return MD_ERR_CAPACITY;
    }
    for (index = 0u; index < portfolio->position_count; index += 1u) {
        if (md_digest_equal(&portfolio->positions[index].lane_id, &position.lane_id)) {
            return MD_ERR_DUPLICATE;
        }
    }
    portfolio->positions[portfolio->position_count] = position;
    portfolio->position_count += 1u;
    return MD_OK;
}

MdStatus md_risk_analyze(const MdRiskPortfolio *portfolio, const MdRiskPolicy *policy, MdAmount available_capital, MdRiskReport *out) {
    MdStatus status;
    size_t index;

    if (portfolio == 0 || out == 0) {
        return MD_ERR_ARGUMENT;
    }
    status = md_risk_policy_validate(policy);
    if (status != MD_OK) {
        return status;
    }
    memset(out, 0, sizeof(*out));
    out->available_capital = available_capital;

    for (index = 0u; index < portfolio->position_count; index += 1u) {
        const MdLaneRiskPosition *position;
        MdAmount expected;
        MdAmount stressed;
        MdAmount gap;

        position = &portfolio->positions[index];
        if (!position->active) {
            continue;
        }
        status = md_expected_loss(position, &expected);
        if (status != MD_OK) {
            return status;
        }
        status = md_amount_mul_bps(position->pending_settlement, policy->stress_default_bps, &stressed);
        if (status != MD_OK) {
            return status;
        }
        status = md_liquidity_gap(position, policy, &gap);
        if (status != MD_OK) {
            return status;
        }
        if ((status = md_risk_add(&out->gross_volume, position->gross_volume)) != MD_OK ||
            (status = md_risk_add(&out->pending_settlement, position->pending_settlement)) != MD_OK ||
            (status = md_risk_add(&out->available_liquidity, position->available_liquidity)) != MD_OK ||
            (status = md_risk_add(&out->expected_loss, expected)) != MD_OK ||
            (status = md_risk_add(&out->stress_loss, stressed)) != MD_OK ||
            (status = md_risk_add(&out->liquidity_gap, gap)) != MD_OK ||
            (status = md_risk_add(&out->fee_reserve, position->fee_reserve)) != MD_OK) {
            return status;
        }
        out->active_positions += 1u;
    }

    if (out->active_positions == 0u) {
        return MD_ERR_STATE;
    }
    for (index = 0u; index < portfolio->position_count; index += 1u) {
        const MdLaneRiskPosition *position;
        uint32_t share_bps;

        position = &portfolio->positions[index];
        if (!position->active) {
            continue;
        }
        share_bps = md_ratio_scaled(position->pending_settlement, out->pending_settlement, MD_BPS_DENOMINATOR);
        if (share_bps > out->largest_lane_share_bps) {
            out->largest_lane_share_bps = share_bps;
        }
        out->hhi_bps += (share_bps * share_bps) / MD_BPS_DENOMINATOR;
        if (share_bps > policy->concentration_threshold_bps) {
            MdAmount excess;
            MdAmount charge;
            status = md_amount_mul_bps(position->pending_settlement, share_bps - policy->concentration_threshold_bps, &excess);
            if (status != MD_OK) {
                return status;
            }
            status = md_amount_mul_bps(excess, policy->concentration_charge_bps, &charge);
            if (status != MD_OK) {
                return status;
            }
            if ((status = md_risk_add(&out->concentration_addon, charge)) != MD_OK) {
                return status;
            }
        }
    }

    out->loss_charge = out->expected_loss > out->stress_loss ? out->expected_loss : out->stress_loss;
    if ((status = md_risk_add(&out->required_capital, out->loss_charge)) != MD_OK ||
        (status = md_risk_add(&out->required_capital, out->liquidity_gap)) != MD_OK ||
        (status = md_risk_add(&out->required_capital, out->fee_reserve)) != MD_OK ||
        (status = md_risk_add(&out->required_capital, out->concentration_addon)) != MD_OK) {
        return status;
    }
    status = md_amount_mul_bps(out->required_capital, policy->minimum_coverage_bps, &out->target_capital);
    if (status != MD_OK) {
        return status;
    }
    out->coverage_bps = md_coverage_bps(available_capital, out->required_capital);
    if (available_capital >= out->target_capital) {
        out->capital_surplus = available_capital - out->target_capital;
    } else {
        out->capital_shortfall = out->target_capital - available_capital;
    }

    out->decision = MD_RISK_APPROVE;
    if (out->capital_shortfall > 0u || out->largest_lane_share_bps > policy->maximum_single_lane_bps ||
        out->hhi_bps > policy->maximum_hhi_bps) {
        out->decision = MD_RISK_HOLD;
    } else if (out->coverage_bps < policy->minimum_coverage_bps + 1000u ||
               out->largest_lane_share_bps + 500u > policy->maximum_single_lane_bps) {
        out->decision = MD_RISK_REVIEW;
    }
    return md_report_digest(out);
}

const char *md_risk_decision_name(MdRiskDecision decision) {
    switch (decision) {
    case MD_RISK_APPROVE:
        return "approve";
    case MD_RISK_REVIEW:
        return "review";
    case MD_RISK_HOLD:
        return "hold";
    default:
        return "unknown";
    }
}
