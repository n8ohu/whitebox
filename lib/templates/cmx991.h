{% extends "base.h" %}

{% block custom %}
uint8_t cmx991_pack(cmx991_t* rf, uint8_t addr);
void cmx991_load(cmx991_t* rf, uint8_t addr, uint32_t data);

double cmx991_pll_actual_frequency(cmx991_t* rf, double fref);
int cmx991_pll_enable(cmx991_t* rf, double fref, double fdes);
int cmx991_pll_enable_m_n(cmx991_t* rf, double fref, int m, int n);
void cmx991_pll_disable(cmx991_t* rf);
int cmx991_pll_locked(cmx991_t* rf);

void cmx991_resume(cmx991_t* rf);
void cmx991_suspend(cmx991_t* rf);
void cmx991_shutdown(cmx991_t* rf);

void cmx991_tx_tune(cmx991_t* rf, float fdes, if_filter_t if_filter,
        hi_lo_t hi_lo, tx_rf_div_t tx_rf_div, tx_if_div_t tx_if_div,
        gain_t gain);
void cmx991_rx_tune(cmx991_t* rf, div_t div, mix_out_t mix_out,
        iq_filter_t iq_filter, vga_t vga);
void cmx991_rx_calibrate(cmx991_t* rf);
{% endblock %}
