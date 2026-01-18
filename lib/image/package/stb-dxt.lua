package("stb_dxt")

	set_urls("https://github.com/Cyan4973/RygsDXTc.git")

	on_install(function(package)
		os.mkdir("stb_dxt")
		os.mv("stb_dxt.h", "stb_dxt/stb_dxt.h")
		io.writefile("xmake.lua", [[
			add_rules("mode.release")
			set_languages("c++11")
			add_vectorexts("sse", "sse2", "avx", "avx2")
			target("stb_dxt")
				set_kind("static")
				add_files("stb_dxt.cpp")
				add_includedirs("stb_dxt")	
				add_headerfiles("(stb_dxt/stb_dxt.h)")
		]])
        import("package.tools.xmake").install(package)
	end)

package_end()