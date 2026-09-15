package("bc7enc")

	set_urls("https://github.com/Stehsaer/bc7enc_rdo/archive/104269f1ef9e3f78c523d83a7d88d4c7eeadb367.zip")

	add_versions("latest", "caa655ce53a45528f32356e713a4381e5e59a76cccfd59ebb25a76d7cedca241")

	on_install(function (package)
        import("package.tools.xmake").install(package)
	end)

package_end()