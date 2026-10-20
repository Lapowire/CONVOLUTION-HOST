import os, pathlib, platform, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class FormatTests(unittest.TestCase):
    def test_unsupported_formats_and_fft_provider_fail_explicitly(self):
        cases=[('CH_FFTW_PROVIDER','provider')]
        if platform.system()!='Darwin':cases.append(('CH_BUILD_AU','AU'))
        if platform.system() not in ('Darwin','Windows'):cases.append(('CH_BUILD_AAX','AAX'))
        for option,word in cases:
            with self.subTest(option=option),tempfile.TemporaryDirectory(prefix='ch-cmake-') as tmp:
                value='unknown' if option=='CH_FFTW_PROVIDER' else 'ON'
                run=subprocess.run(['cmake','-S',str(ROOT),'-B',tmp,'-DCH_BUILD_CLAP=OFF',f'-D{option}={value}'],capture_output=True,text=True)
                self.assertNotEqual(run.returncode,0,run.stdout+run.stderr)
                self.assertIn(word,(run.stdout+run.stderr))
    def test_incomplete_aax_sdk_fails_explicitly(self):
        run=subprocess.run(['cmake','-P',str(ROOT/'tests/cmake/FormatOptions.cmake')],capture_output=True,text=True)
        self.assertNotEqual(run.returncode,0)
        self.assertIn('AAX SDK is incomplete',run.stdout+run.stderr)
if __name__=='__main__':unittest.main()
