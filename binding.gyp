{
  'variables': {
    'module_name': 'sapnwrfc',
    'library': 'shared_library',
    'target_arch': 'ia32',
    'output_directory': 'Release',
  },

  'targets': [{
    'sources': [
      'src/binding.cc',
      'src/Common.h',
      'src/Connection.h',
      'src/Connection.cc',
      'src/Function.h',
      'src/Function.cc',
    ],
    
    'target_name': '<(module_name)',
    'type': '<(library)',
    'product_name': '<(module_name)',
    'product_extension': 'node',
    'product_prefix': '',
    
    'defines': [
      'ARCH="<(target_arch)"',
      'PLATFORM="<(OS)"',
      '_LARGEFILE_SOURCE',
      '_FILE_OFFSET_BITS=64',
      'SAPwithUNICODE',
      'SAPwithTHREADS',
      'NDEBUG',
    ],
    
    'conditions': [
      [ 'OS=="win"', {
        'defines': [
          'PLATFORM="win32"',
          '_X86_',
          'WIN32',
          '_AFXDLL',
          '_CRT_NON_CONFORMING_SWPRINTFS',
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_DEPRECATE',
          'SAPonNT',
          'UNICODE',
          '_UNICODE'
        ],
        'include_dirs': [
          '<(module_root_dir)/nwrfcsdk/include',
        ],
        'msvs_configuration_attributes': {
          'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj'
        },
        'msvs-settings': {
          'VCLinkerTool': {
            'SubSystem': 3, # /subsystem:dll
            'AdditionalLibraryDirectories': '<(module_root_dir)/nwrfcsdk/lib',
            'AdditionalDependencies': 'sapnwrfc.lib;sapucum.lib',
          },
        },
       'libraries': [ '-lsapnwrfc.lib', '-llibsapucum.lib' ],
       'product_dir': '<(output_directory)'
      }],
      
      [ 'OS=="mac"', {
        'defines': [
          'SAPonUNIX',
          'SAPwithTHREADS',
          '__NO_MATH_INLINES'
        ],
        'conditions': [
          [ 'target_arch=="ia32"' , {
            'xcode_settings' : {
              'ARCHS': ['i386'],
              'OTHER_CFLAGS': ['-m32'],
              'OTHER_CXX_FLAGS': ['-m32'],
              'OTHER_LDFLAGS': ['-m32']
            }
          }],
          [ 'target_arch=="x64"' , {
            'xcode_settings' : {
              'ARCHS': ['x86_64'],
              'OTHER_CFLAGS': ['-m64'],
              'OTHER_CXX_FLAGS': ['-m64'],
              'OTHER_LDFLAGS': ['-m64']
            }
          }],
        ],
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '3',
          'WARNING_CFLAGS!': [
            '-Wall',
            '-W'
          ]
        }
      }],
      
      [ 'OS=="linux"', {
        'cflags!': [
          '-Wall',
          '-pedantic'
        ],
        'include_dirs': [
          '/usr/local/include'
        ],
        'defines': [
          'SAPonUNIX',
          'SAPonLIN',
          'SAPwithTHREADS',
          '__NO_MATH_INLINES'
        ],
        'libraries': [ '-lsapnwrfc', '-lsapucum' ],
        'product_dir': '<(output_directory)'
      }]
    ],
  }] # end targets
}
