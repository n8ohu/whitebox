"""
Whitebox SoC Peripheral
=======================
"""
from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        toVerilog

from fir import fir as FIR
from dsp import Signature
from duc import duc as DUC
from rfe import rfe as RFE
from rfe import print_rfe_ioctl
from ram import Ram, Ram2
from apb3_utils import Apb3Bus

class OverrunError(Exception):
    """Thrown when the system experiences an overflow on a FIFO buffer."""
    pass

class UnderrunError(Exception):
    """Thrown when the system experiences an underflow on a FIFO buffer."""
    pass

def whitebox_reset(resetn,
                  dac_clock,
                  clear_enable,
                  clearn):

    state_t = enum('CLEAR', 'CLEARING', 'RUN')
    state = Signal(state_t.CLEAR)

    sync_clear_en = Signal(bool(0))
    clear_en = Signal(bool(0))
    clear_count = Signal(intbv(0)[10:])

    @always_seq(dac_clock.posedge, reset=resetn)
    def controller():
        sync_clear_en.next = clear_enable
        clear_en.next = sync_clear_en
        if state == state_t.CLEAR:
            clearn.next = 0
            clear_count.next = 16
            state.next = state_t.CLEARING
        elif state == state_t.CLEARING:
            clear_count.next = clear_count - 1
            if clear_count == 0:
                state.next = state_t.RUN
            else:
                state.next = state_t.CLEARING
        if state == state_t.RUN:
            clearn.next = 1
            if clear_en:
                state.next = state_t.CLEAR
    
    return controller

def whitebox(
        resetn,
        pclk,
        paddr,
        psel,
        penable,
        pwrite,
        pwdata,
        pready,
        prdata,
        #pslverr,
        clearn,
        clear_enable,
        dac_clock,
        dac2x_clock,
        dac_en,
        dac_data,
        adc_idata,
        adc_qdata,
        tx_status_led,
        tx_dmaready,
        rx_status_led,
        rx_dmaready,
        tx_fifo_re,
        tx_fifo_rdata,
        tx_fifo_we,
        tx_fifo_wdata,
        tx_fifo_full,
        tx_fifo_afull,
        tx_fifo_empty,
        tx_fifo_aempty,
        tx_fifo_afval,
        tx_fifo_aeval,
        tx_fifo_wack,
        tx_fifo_dvld,
        tx_fifo_overflow,
        tx_fifo_underflow,
        tx_fifo_rdcnt,
        tx_fifo_wrcnt,
        rx_fifo_re,
        rx_fifo_rdata,
        rx_fifo_we,
        rx_fifo_wdata,
        rx_fifo_full,
        rx_fifo_afull,
        rx_fifo_empty,
        rx_fifo_aempty,
        rx_fifo_afval,
        rx_fifo_aeval,
        rx_fifo_wack,
        rx_fifo_dvld,
        rx_fifo_overflow,
        rx_fifo_underflow,
        rx_fifo_rdcnt,
        rx_fifo_wrcnt,
        fir_coeff_ram_addr,
        fir_coeff_ram_din0,
        fir_coeff_ram_din1,
        fir_coeff_ram_blk,
        fir_coeff_ram_wen,
        fir_coeff_ram_dout0,
        fir_coeff_ram_dout1,
        fir_load_coeff_ram_addr,
        fir_load_coeff_ram_din0,
        fir_load_coeff_ram_din1,
        fir_load_coeff_ram_blk,
        fir_load_coeff_ram_wen,
        fir_load_coeff_ram_dout0,
        fir_load_coeff_ram_dout1,
        fir_delay_line_i_ram_addr,
        fir_delay_line_i_ram_din,
        fir_delay_line_i_ram_blk,
        fir_delay_line_i_ram_wen,
        fir_delay_line_i_ram_dout,
        fir_delay_line_q_ram_addr,
        fir_delay_line_q_ram_din,
        fir_delay_line_q_ram_blk,
        fir_delay_line_q_ram_wen,
        fir_delay_line_q_ram_dout,
        **kwargs):
    """The whitebox.

    :param resetn: Reset the whole radio front end.
    :param clearn: Clear the DSP Chain
    :param dac_clock: Clock running at DAC rate
    :param dac2x_clock: Clock running at double DAC rate
    :param pclk: The system bus clock
    :param paddr: The bus assdress
    :param psel: The bus slave select
    :param penable: The bus slave enable line
    :param pwrite: The bus read/write flag
    :param pwdata: The bus write data
    :param pready: The bus slave ready signal
    :param prdata: The bus read data
    :param pslverr: The bus slave error flag
    :param dac_clock: The DAC clock
    :param dac_data: The DAC data
    :param dac_en: Enable DAC output
    :param status_led: Output pin for whitebox status
    :param dmaready: Ready signal to DMA controller
    :param txirq: Almost empty interrupt to CPU
    :param clear_enable: To reset controller, set this high for reset
    """
    dspsim = kwargs.get('dspsim', None)
    interp_default = kwargs.get('interp', 1)
    fcw_bitwidth = kwargs.get('fcw_bitwidth', 25)

    ######### VARS AND FLAGS ###########
    print 'interp=', interp_default

    interp = Signal(intbv(interp_default)[11:])
    shift = Signal(intbv(0, min=0, max=21))
    firen = Signal(bool(0))
    fir_bank1 = Signal(bool(0))
    fir_bank0 = Signal(bool(0))
    fir_N = Signal(intbv(0, min=0, max=2**7))

    tx_correct_i = Signal(intbv(0, min=-2**9, max=2**9))
    tx_correct_q = Signal(intbv(0, min=-2**9, max=2**9))
    tx_gain_i = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    tx_gain_q = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    fcw = Signal(intbv(1)[fcw_bitwidth:])
    txen = Signal(bool(0))
    txstop = Signal(bool(0))
    txfilteren = Signal(bool(0))
    ddsen = Signal(bool(False))
    loopen = Signal(bool(False))

    decim = Signal(intbv(interp_default)[11:])
    rx_correct_i = Signal(intbv(0, min=-2**9, max=2**9))
    rx_correct_q = Signal(intbv(0, min=-2**9, max=2**9))
    rxen = Signal(bool(0))
    rxstop = Signal(bool(0))
    rxfilteren = Signal(bool(0))

    ########### DIGITAL SIGNAL PROCESSING #######
    loopback = Signature("loopback", False, bits=10)
    duc_underrun = Signal(modbv(0, min=0, max=2**16))
    dac_last = Signal(bool(0))

    ddc_overrun = Signal(modbv(0, min=0, max=2**16))
    ddc_flags = Signal(intbv(0)[4:])
    adc_last = Signal(bool(0))

    tx_sample = Signature("tx_sample", True, bits=16)
    tx_sample_valid = tx_sample.valid
    tx_sample_last = tx_sample.last
    tx_sample_i = tx_sample.i
    tx_sample_q = tx_sample.q

    rx_sample = Signature("rx_sample", True, bits=16)
    rx_sample_valid = rx_sample.valid
    rx_sample_last = rx_sample.last
    rx_sample_i = rx_sample.i
    rx_sample_q = rx_sample.q

    duc_args = (clearn, dac_clock, dac2x_clock,
            loopen, loopback,
            tx_fifo_empty, tx_fifo_re, tx_fifo_dvld, tx_fifo_rdata, tx_fifo_underflow,
            txen, txstop,
            ddsen, txfilteren,
            interp, shift,
            fcw,
            tx_correct_i, tx_correct_q,
            tx_gain_i, tx_gain_q,
            duc_underrun, tx_sample,
            dac_en, dac_data, dac_last,

            rx_fifo_full, rx_fifo_we, rx_fifo_wdata,
            rxen, rxstop, rxfilteren,
            decim, rx_correct_i, rx_correct_q,
            ddc_overrun, rx_sample,
            adc_idata, adc_qdata, adc_last,

            fir_coeff_ram_addr,
            fir_coeff_ram_din0,
            fir_coeff_ram_din1,
            fir_coeff_ram_blk,
            fir_coeff_ram_wen,
            fir_coeff_ram_dout0,
            fir_coeff_ram_dout1,
            fir_delay_line_i_ram_addr,
            fir_delay_line_i_ram_din,
            fir_delay_line_i_ram_blk,
            fir_delay_line_i_ram_wen,
            fir_delay_line_i_ram_dout,
            fir_delay_line_q_ram_addr,
            fir_delay_line_q_ram_din,
            fir_delay_line_q_ram_blk,
            fir_delay_line_q_ram_wen,
            fir_delay_line_q_ram_dout,
            firen, fir_bank1, fir_bank0, fir_N,)

    duc_kwargs = dict(dspsim=dspsim,
                    interp=interp_default,
                    cic_enable=kwargs.get('cic_enable', True),
                    fir_enable=kwargs.get('fir_enable', True),
                    dds_enable=kwargs.get('dds_enable', True),
                    conditioning_enable=kwargs.get('conditioning_enable', True))
    if kwargs.get("duc_enable", True):
        duc = DUC(*duc_args, **duc_kwargs)
    else:
        duc = None

    ########### RADIO FRONT END ##############
    rfe_args = (resetn,
        pclk, paddr, psel, penable, pwrite, pwdata, pready, prdata, #pslverr,
        clearn, clear_enable, loopen,

        tx_status_led, tx_dmaready,
        rx_status_led, rx_dmaready,
        tx_fifo_we, tx_fifo_wdata,
        tx_fifo_empty, tx_fifo_full,
        tx_fifo_afval, tx_fifo_aeval, tx_fifo_afull, tx_fifo_aempty,
        tx_fifo_wack, tx_fifo_dvld,
        tx_fifo_overflow, tx_fifo_underflow,
        tx_fifo_rdcnt, tx_fifo_wrcnt,

        rx_fifo_re, rx_fifo_rdata,
        rx_fifo_empty, rx_fifo_full,
        rx_fifo_afval, rx_fifo_aeval, rx_fifo_afull, rx_fifo_aempty,
        rx_fifo_wack, rx_fifo_dvld,
        rx_fifo_overflow, rx_fifo_underflow,
        rx_fifo_rdcnt, rx_fifo_wrcnt,

        fir_load_coeff_ram_addr,
        fir_load_coeff_ram_din0,
        fir_load_coeff_ram_din1,
        fir_load_coeff_ram_blk,
        fir_load_coeff_ram_wen,
        fir_load_coeff_ram_dout0,
        fir_load_coeff_ram_dout1,

        firen, fir_bank1, fir_bank0, fir_N,

        interp, shift,
        fcw, tx_correct_i, tx_correct_q, tx_gain_i, tx_gain_q,
        txen, txstop, ddsen, txfilteren,
        decim, rx_correct_i, rx_correct_q,
        rxen, rxstop, rxfilteren,
        duc_underrun, dac_last,
        ddc_overrun, adc_last)

    rfe = RFE(*rfe_args)

    return rfe, duc

def main():
    #w = Whitebox()
    #whitebox_v = toVerilog(whitebox, **w.signals_dict())

    print_rfe_ioctl()  # TODO: this is a generator of ioctl.h which is part of the driver.

    return 0

if __name__ == '__main__':
    main()
