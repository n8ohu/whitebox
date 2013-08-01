from datetime import datetime
import math

import jinja2

class BitField(object):
    def __init__(self, addr, bit_range, default=None, enum=None):
        self.addr = addr
        self._default = default if default else 0
        if type(bit_range) == int:
            self.bit_range = (bit_range, bit_range)
        elif type(bit_range) in (list, tuple) and len(bit_range) == 2:
            self.bit_range = (int(bit_range[0]), int(bit_range[1]))
        else:
            raise ValueError, 'Invalid bit_range'

        self.enum = None
        if enum:
            self.enum = []
            if type(enum) in (list, tuple):
                enum_val = 0
                for e in enum:
                    self.enum.append((e, enum_val))
                    enum_val += 1
            elif type(enum) == dict:
                self.enum = enum.items()
            else:
                raise ValueError, 'Invalid enum'

    @property
    def default(self):
        if self.enum:
            for key, val in self.enum:
                if val == self._default or key == self._default:
                    return ('%s_%s' % (self.name, key)).upper()
        return self._default
    
    @property
    def type(self):
        if self.enum:
            return '%s_t' % self.name
        return 'uint%d_t' % max(2**math.ceil(math.log(self.bit_width, 2)), 8)

    @property
    def shift(self):
        return self.bit_range[0]

    @property
    def mask(self):
        return hex(int('1' * self.bit_width, 2))

    @property
    def bit_width(self):
        return self.bit_range[1] - self.bit_range[0] + 1

class Register(object):
    def __init__(self, *bit_fields):
        self.bit_fields = bit_fields

    @property
    def bit_width(self):
        return sum([b.bit_width for b in self.bit_fields])

    @property
    def type(self):
        return 'uint%d_t' % max(2**math.ceil(math.log(self.bit_width, 2)), 8)

class MetaModel(type):
    def __new__(cls, name, bases, attrs):
        newattrs = {'bit_fields': [], 'registers': []}
        for attrname, attrvalue in attrs.iteritems():
            newattrs[attrname] = attrvalue
            # Set the name of BitFields
            if type(attrvalue) in (BitField, Register):
                newattrs[attrname].name = attrname
            if type(attrvalue) == BitField:
                newattrs['bit_fields'].append(attrvalue)
            if type(attrvalue) == Register:
                newattrs['registers'].append(attrvalue)
        return super(MetaModel, cls).__new__(cls, name, bases, newattrs)

class Model(object):
    __metaclass__ = MetaModel
    template = 'base'

def render(output_filename, template, model):
    from jinja2 import Environment, PackageLoader
    env = Environment(loader=PackageLoader('util', 'lib', 'templates'), autoescape=False, trim_blocks=True, lstrip_blocks=True)

    model_name = model.__class__.__name__.lower()
    render_dict = {
        'name': model_name,
        'model': model,
        'time': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
    }
    with open(output_filename, 'w') as f:
        t = env.get_template(template)
        f.write(t.render(render_dict))

