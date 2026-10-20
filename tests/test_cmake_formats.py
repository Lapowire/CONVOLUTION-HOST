import os, pathlib, platform, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class FormatTests(unittest.TestCase):
    def test_unsupported_formats_and_fft_provider_fail_explicitly(self):
        cases = [("CH_FFTW_PROVIDER", "provider")]
        if platform.system() != "Darwin":
            cases.append(("CH_BUILD_AU", "AU"))
        if platform.system() not in ("Darwin", "Windows"):
            cases.append(("CH_BUILD_AAX", "AAX"))
        for option, word in cases:
            with self.subTest(option=option), tempfile.TemporaryDirectory(
                prefix="ch-cmake-"
            ) as tmp:
                value = "unknown" if option == "CH_FFTW_PROVIDER" else "ON"
                run = subprocess.run(
                    [
                        "cmake",
                        "-S",
                        str(ROOT),
                        "-B",
                        tmp,
                        "-DCH_BUILD_CLAP=OFF",
                        f"-D{option}={value}",
                    ],
                    capture_output=True,
                    text=True,
                )
                self.assertNotEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertIn(word, (run.stdout + run.stderr))

    def test_fftw_bigobj_is_an_option_instead_of_a_definition(self):
        with tempfile.TemporaryDirectory(prefix="ch-fftw-flags-") as tmp:
            source = pathlib.Path(tmp)
            (source / "dummy.c").write_text("int dummy(void) { return 0; }\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.22)\nproject(Flags C)\n"
                "add_library(fake STATIC dummy.c)\n"
                "target_compile_definitions(fake PRIVATE KEEP=1 /bigobj)\n"
                f'include("{ROOT.as_posix()}/cmake/FftCompilerOptions.cmake")\n'
                "ch_fix_fftw_msvc_flags(fake)\n"
                "get_target_property(defs fake COMPILE_DEFINITIONS)\n"
                "get_target_property(opts fake COMPILE_OPTIONS)\n"
                'if("/bigobj" IN_LIST defs OR NOT "/bigobj" IN_LIST opts OR NOT "KEEP=1" IN_LIST defs)\n'
                'message(FATAL_ERROR "Malformed FFTW compiler flags")\nendif()\n'
            )
            run = subprocess.run(
                ["cmake", "-S", tmp, "-B", str(source / "out")],
                capture_output=True,
                text=True,
            )
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_incomplete_aax_sdk_fails_explicitly(self):
        run = subprocess.run(
            ["cmake", "-P", str(ROOT / "tests/cmake/FormatOptions.cmake")],
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("AAX SDK is incomplete", run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
