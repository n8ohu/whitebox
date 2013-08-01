from util import mvic

def _get_class(module, classname):
    try:
        klass = getattr(module, classname)
    except AttributeError:
        return None
    return klass

def generate(f):
    import os.path
    import importlib
    dirs, filename = os.path.split(f)
    fname, extension = os.path.splitext(filename)
    print dirs, fname, extension
    if extension in ('.c', '.h'):
        optional_import = dirs
        model = fname

        import sys
        # TODO: replace this with a settings
        WHITEBOX_PATH = '/home/testa/whitebox'
        sys.path.append(os.path.join(WHITEBOX_PATH, dirs))
        models = importlib.import_module('models')
        klass = None
        names = [model, model.lower(), model.upper(), model.capitalize()]
        while klass is None and names:
            klass = _get_class(models, names.pop())
        if klass is None:
            raise Exception, 'Unknown class %s!' % fname
        m = klass()
        mvic.render(filename, m.template + extension, m)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Generate MViC')
    parser.add_argument('files', metavar='file', type=str, nargs='+',
            help='Files to generate.')
    args = parser.parse_args()
    for f in args.files:
        generate(f)
