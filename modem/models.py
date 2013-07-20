from mvic import Model, BitField, Register


class Modem(Model):
    guarded = BitField(0, (0, 1), 'off', ('off', 'on'))
    m0 = BitField(1, (0, 8))
    m1 = BitField(2, (0, 8))
    m = Register(m0, m1)

