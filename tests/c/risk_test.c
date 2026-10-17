#include "meridian/risk.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static MdLaneRiskPosition position(const char *label, MdAmount gross, MdAmount pending, MdAmount liquidity, MdAmount reserve, uint32_t probability, uint32_t recovery) {
    MdLaneRiskPosition value;

    memset(&value, 0, sizeof(value));
    value.lane_id = md_digest_text("risk-test-lane", label);
    snprintf(value.label, sizeof(value.label), "%s", label);
    value.gross_volume = gross;
    value.pending_settlement = pending;
    value.available_liquidity = liquidity;
    value.fee_reserve = reserve;
    value.default_probability_bps = probability;
    value.recovery_bps = recovery;
    value.active = 1;
    return value;
}

static MdRiskPortfolio diversified_portfolio(void) {
    MdRiskPortfolio portfolio;

    md_risk_portfolio_init(&portfolio);
    assert(md_risk_portfolio_add(&portfolio, position("north", 900000u, 300000u, 290000u, 2000u, 120u, 6500u)) == MD_OK);
    assert(md_risk_portfolio_add(&portfolio, position("south", 850000u, 280000u, 270000u, 1800u, 140u, 6000u)) == MD_OK);
    assert(md_risk_portfolio_add(&portfolio, position("relay", 700000u, 220000u, 215000u, 1500u, 180u, 5500u)) == MD_OK);
    assert(md_risk_portfolio_add(&portfolio, position("archive", 620000u, 200000u, 195000u, 1200u, 100u, 7000u)) == MD_OK);
    return portfolio;
}

static void test_policy_validation(void) {
    MdRiskPolicy policy;

    policy = md_risk_policy_operating();
    assert(md_risk_policy_validate(&policy) == MD_OK);
    policy.liquidity_haircut_bps = 10001u;
    assert(md_risk_policy_validate(&policy) == MD_ERR_POLICY);
    assert(md_risk_policy_validate(0) == MD_ERR_ARGUMENT);
}

static void test_portfolio_admission(void) {
    MdRiskPortfolio portfolio;
    MdLaneRiskPosition lane;

    md_risk_portfolio_init(&portfolio);
    lane = position("north", 1000u, 500u, 500u, 10u, 100u, 5000u);
    assert(md_risk_portfolio_add(&portfolio, lane) == MD_OK);
    assert(md_risk_portfolio_add(&portfolio, lane) == MD_ERR_DUPLICATE);
    lane.recovery_bps = 10001u;
    assert(md_risk_portfolio_add(&portfolio, lane) == MD_ERR_ARGUMENT);
}

static void test_operating_report(void) {
    MdRiskPortfolio portfolio;
    MdRiskReport report;
    MdRiskPolicy policy;

    portfolio = diversified_portfolio();
    policy = md_risk_policy_operating();
    assert(md_risk_analyze(&portfolio, &policy, 600000u, &report) == MD_OK);
    assert(report.gross_volume == 3070000u);
    assert(report.pending_settlement == 1000000u);
    assert(report.active_positions == 4u);
    assert(report.expected_loss > 0u);
    assert(report.stress_loss > report.expected_loss);
    assert(report.loss_charge == report.stress_loss);
    assert(report.required_capital > report.loss_charge);
    assert(report.target_capital > report.required_capital);
    assert(report.coverage_bps > 10000u);
    assert(strlen(report.digest.hex) == MD_DIGEST_HEX_LENGTH);
}

static void test_severe_report_requires_more_capital(void) {
    MdRiskPortfolio portfolio;
    MdRiskReport operating;
    MdRiskReport severe;
    MdRiskPolicy operating_policy;
    MdRiskPolicy severe_policy;

    portfolio = diversified_portfolio();
    operating_policy = md_risk_policy_operating();
    severe_policy = md_risk_policy_severe();
    assert(md_risk_analyze(&portfolio, &operating_policy, 600000u, &operating) == MD_OK);
    assert(md_risk_analyze(&portfolio, &severe_policy, 600000u, &severe) == MD_OK);
    assert(severe.stress_loss > operating.stress_loss);
    assert(severe.liquidity_gap > operating.liquidity_gap);
    assert(severe.required_capital > operating.required_capital);
    assert(severe.target_capital > operating.target_capital);
}

static void test_concentration_hold(void) {
    MdRiskPortfolio portfolio;
    MdRiskReport report;
    MdRiskPolicy policy;

    md_risk_portfolio_init(&portfolio);
    assert(md_risk_portfolio_add(&portfolio, position("dominant", 1000000u, 900000u, 850000u, 1000u, 200u, 5000u)) == MD_OK);
    assert(md_risk_portfolio_add(&portfolio, position("small", 100000u, 100000u, 100000u, 100u, 100u, 7000u)) == MD_OK);
    policy = md_risk_policy_operating();
    assert(md_risk_analyze(&portfolio, &policy, 1000000u, &report) == MD_OK);
    assert(report.largest_lane_share_bps == 9000u);
    assert(report.hhi_bps > policy.maximum_hhi_bps);
    assert(report.concentration_addon > 0u);
    assert(report.decision == MD_RISK_HOLD);
}

static void test_inactive_position_is_ignored(void) {
    MdRiskPortfolio portfolio;
    MdLaneRiskPosition inactive;
    MdRiskReport report;
    MdRiskPolicy policy;

    md_risk_portfolio_init(&portfolio);
    assert(md_risk_portfolio_add(&portfolio, position("active", 1000u, 500u, 500u, 0u, 100u, 5000u)) == MD_OK);
    inactive = position("inactive", 999999u, 999999u, 0u, 0u, 10000u, 0u);
    inactive.active = 0;
    assert(md_risk_portfolio_add(&portfolio, inactive) == MD_OK);
    policy = md_risk_policy_operating();
    assert(md_risk_analyze(&portfolio, &policy, 1000u, &report) == MD_OK);
    assert(report.active_positions == 1u);
    assert(report.gross_volume == 1000u);
}

static void test_empty_portfolio_rejected(void) {
    MdRiskPortfolio portfolio;
    MdRiskReport report;
    MdRiskPolicy policy;

    md_risk_portfolio_init(&portfolio);
    policy = md_risk_policy_operating();
    assert(md_risk_analyze(&portfolio, &policy, 0u, &report) == MD_ERR_STATE);
}

static void test_digest_is_deterministic(void) {
    MdRiskPortfolio portfolio;
    MdRiskReport left;
    MdRiskReport right;
    MdRiskPolicy policy;

    portfolio = diversified_portfolio();
    policy = md_risk_policy_operating();
    assert(md_risk_analyze(&portfolio, &policy, 500000u, &left) == MD_OK);
    assert(md_risk_analyze(&portfolio, &policy, 500000u, &right) == MD_OK);
    assert(md_digest_equal(&left.digest, &right.digest));
}

int main(void) {
    test_policy_validation();
    test_portfolio_admission();
    test_operating_report();
    test_severe_report_requires_more_capital();
    test_concentration_hold();
    test_inactive_position_is_ignored();
    test_empty_portfolio_rejected();
    test_digest_is_deterministic();
    puts("risk tests: 8 passed");
    return 0;
}
