# Solomon Systech SSD1306 Oled Driver

ssd1306_files = files(
	'ssd1306.cpp'
)

ssd1306 = static_library('ssd1306',
	sources: ssd1306_files,
	dependencies: [
		framework_include_dep,
		framework_host_include_dep
	],
	build_by_default: meson.is_subproject() == false
)

ssd1306_native = static_library('ssd1306_native',
	sources: ssd1306_files,
	dependencies: [
		framework_include_dep,
		framework_native_include_dep
	],
	native: true,
	build_by_default: meson.is_subproject() == false
)

clangtidy_files += ssd1306_files
