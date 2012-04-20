import os
import Options
from os.path import abspath, join
import shutil

srcdir = os.path.abspath('.')
blddir = os.path.abspath('build')
cwd = os.getcwd()

VERSION = '0.1.0'

def set_options(opt):
  opt.tool_options('compiler_cxx')

  opt.add_option( '--sapnwrfcsdk'
    , action='store'
    , type='string'
    , default=False
    , help='Location of the SAP NetWeaver RFC SDK [Default: ' + os.path.abspath(srcdir) + '/nwrfcsdk]'
    )

def configure(conf):

  o = Options.options

  if o.sapnwrfcsdk:
    sapnwrfcsdk_path = os.path.abspath(o.sapnwrfcsdk)
  else:
    sapnwrfcsdk_path = os.path.abspath(os.path.join(srcdir, 'nwrfcsdk'))

  sapnwrfcsdk_includes = os.path.join(sapnwrfcsdk_path, 'include')
  sapnwrfcsdk_libpath = os.path.join(sapnwrfcsdk_path, 'lib')
  conf.env['SAPNWRFCSDK_LIBPATH'] = sapnwrfcsdk_libpath

  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')
  
  # SAP NW RFC SDK
  result = conf.check(lib='sapnwrfc',
                      header_name="sapnwrfc.h",
                      includes = sapnwrfcsdk_includes,
                      libpath = sapnwrfcsdk_libpath,
                      uselib_store = 'NWRFCSDK')
  if not result:
    conf.fatal('\n\nDownload, extract and copy the SAP NW RFC SDK to your system lib/include directories\n OR \nuse the --sapnwrfcsdk option')

  result = conf.check(lib='sapucum',
                      includes = sapnwrfcsdk_includes,
                      libpath = sapnwrfcsdk_libpath,
                      uselib_store = 'NWRFCSDK')
  if not result:
    conf.fatal('\n\nDownload, extract and copy the SAP NW RFC SDK to your system lib/include directories\n OR \nuse the --sapnwrfcsdk option')

  # CXX/Linker flags
  cxx_flags = '-O2 -g -fexceptions -funsigned-char -fno-strict-aliasing -fPIC -pthread -minline-all-stringops'
  cxx_flags += ' -DNDEBUG -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DSAPonUNIX -DSAPwithUNICODE -D__NO_MATH_INLINES -DSAPwithTHREADS -DSAPonLIN'
  if conf.env['DEST_CPU'] == 'x86_64':
    cxx_flags += ' -m64 -fno-omit-frame-pointer'
  else:
    cxx_flags += ' -march=i686'
  conf.env.append_value('CXXFLAGS', cxx_flags.split())
  
  linkflags = '-Wl,-R' + os.path.join(srcdir, 'nwrfcsdk/lib')
  conf.env.append_value('LINKFLAGS', linkflags.split())
  
def build(bld):

  o = Options.options
  
  obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
  obj.target = 'sapnwrfc'
  obj.source = 'src/binding.cc src/Connection.cc src/Function.cc'
  obj.uselib = 'NWRFCSDK'
  obj.install_path = srcdir

  
  if (bld.is_install > 0) and (o.sapnwrfcsdk):
    shutil.copytree(bld.env['SAPNWRFCSDK_LIBPATH'], os.path.join(srcdir, 'nwrfcsdk/lib'))
