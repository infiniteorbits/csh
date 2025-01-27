project('csh', 'c', subproject_dir: 'lib', default_options: [
	'buildtype=debug', 
	'c_std=gnu11', 
	'b_lto=false',
	'default_library=static',
	'csp:packet_padding_bytes=42', 
	'csp:buffer_count=1000',
	'csp:buffer_size=2048',
	'csp:conn_max=20',
	'csp:conn_rxqueue_len=1000',
	'csp:qfifo_len=1000',
	'csp:rdp_max_window=1000',
	'csp:port_max_bind=16',
	'param:have_fopen=true', 
	'param:slash=true', 
	'param:commands=true',
	'param:scheduler=true',
	'param:commands_client=true',
	'param:scheduler_client=true',
	'param:collector=false', 
	'param:list_dynamic=true'])

add_global_arguments('-I../include', language: 'c')

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: true).as_link_whole()
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()
libelf_dep = dependency('libelf', required: false)

csh_sources = [
	'src/main.c',
	'src/slash_csp.c',
	'src/csp_debug_param.c',
	'src/base16.c',
	'src/param_sniffer.c',
	'src/prometheus.c',
	'src/resbuf_dump.c',
	'src/stdbuf_mon.c',
	'src/stdbuf_client.c',
	'src/randombytes.c',
	'src/csp_if_eth.c',
	'src/csp_scan.c',
	'src/spaceboot_slash.c',
	'src/nav.c',
	'src/serial.c',
	'src/known_hosts.c',
	'src/hk_retrieve.c',
	'src/battest_slash.c',
	'src/rewl_test.c',
]

if libelf_dep.found()
	csh_sources += 'src/elfparse.c'
endif

csh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--always', '--dirty=+'])


csh = executable('csh', csh_sources,
	dependencies : [slash_dep, csp_dep, param_dep, libelf_dep],
	link_args : ['-Wl,-Map=csh.map', '-lm'],
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), csh.full_path()], depends: csh, build_by_default: true)

zmqproxy_sources = ['src/zmqproxy.c']
zmqproxy = executable('zmqproxy', zmqproxy_sources,
	dependencies : [csp_dep],
	install : true,
)

spacebridge_sources = ['src/spacebridge.c']
spacebridge = executable('spacebridge', spacebridge_sources,
	dependencies : [csp_dep],
	install : true,
)

install_data('yaml/caninit', install_dir : get_option('bindir'))
