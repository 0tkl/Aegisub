conf_pkg = configuration_data()

if host_machine.system() == 'windows'
    installer_setup = find_program(meson.project_source_root() / 'tools/win-installer-setup.ps1')
    run_target('win-installer',
               command: [installer_setup, meson.project_build_root(), meson.project_source_root()])
    portable_setup = find_program(meson.project_source_root() / 'packages/win_installer/portable/create-portable.ps1')
    run_target('win-portable', command: [portable_setup, meson.project_build_root(), meson.project_source_root()])
elif host_machine.system() == 'darwin'
    fontconfig_conf = run_command('pkg-config', '--variable=confdir', 'fontconfig').stdout().strip()
    bundle_app_sh = find_program(meson.project_source_root() / 'tools/osx-bundle.sh')
    run_target('osx-bundle',
               command: [bundle_app_sh, meson.project_source_root(), meson.project_build_root(), 'wx-config', fontconfig_conf, '',
                         get_option('build_osx_bundle') ? 'TRUE' : 'FALSE'])
    build_dmg_sh = find_program(meson.project_source_root() / 'tools/osx-dmg.sh')
    run_target('osx-build-dmg',
               command: [build_dmg_sh, meson.project_source_root(), meson.project_build_root(), meson.project_version()])
else
    conf_pkg.set('AEGISUB_COMMAND', 'aegisub')

    desktop_template = configure_file(input: 'desktop/aegisub.desktop.in.in',
                                      output: 'aegisub.desktop.in',
                                      configuration: conf_pkg)

    i18n = import('i18n')
    i18n.merge_file(input: desktop_template,
                    output: 'org.arch1t3cht.aegisub.desktop',
                    type: 'desktop',
                    po_dir: '../po',
                    install: true,
                    install_dir: datadir / 'applications')

	appdata_template = configure_file(input: 'desktop/aegisub.appdata.xml.in.in',
                                      output: 'aegisub.desktop.appdata.xml.in',
                                      configuration: conf_pkg)
	i18n.merge_file(input: appdata_template,
                    output: 'org.arch1t3cht.aegisub.appdata.xml',
                    type: 'xml',
                    po_dir: '../po',
                    install: true,
                    install_dir: datadir / 'metainfo')

    aegisub_logos = ['16x16.png', '22x22.png', '24x24.png', '32x32.png', '48x48.png', '64x64.png', 'scalable.svg']

    foreach s: aegisub_logos
        dir = s.split('.')[0]
        ext = s.split('.')[1]
        install_data('desktop' / dir / 'aegisub.' + ext,
                     install_dir: datadir / 'icons' / 'hicolor' / dir / 'apps')
    endforeach

    if get_option('build_appimage')
      install_symlink('AppRun', install_dir: '/', pointing_to: bindir.strip('/') / 'aegisub')
      install_symlink('.DirIcon', install_dir: '/', pointing_to: datadir.strip('/') / 'icons' / 'hicolor' / 'scalable' / 'apps' / 'aegisub.svg')
      install_symlink('org.arch1t3cht.aegisub.desktop', install_dir: '/', pointing_to: datadir.strip('/') / 'applications' / 'org.arch1t3cht.aegisub.desktop')
    endif
endif
