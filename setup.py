from distutils.core import setup, Extension

libsgutils = Extension('sgutils',
                    sources = ['sg3utils.c'],
                    libraries= ['sgutils2'])

setup (name = 'sgutils',
       version = '0.1',
       description = 'Python binding for sg3_utils',
       maintainer='Andy Grover',
       maintainer_email='andy@groveronline.com',
       url='http://github.com/agrover/python-sgutils',
       ext_modules = [libsgutils])
