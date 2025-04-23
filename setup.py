# -*- coding: utf-8 -*-


from setuptools import setup, find_packages, Extension

from codecs import open
from os.path import abspath
from sys import argv, byteorder, maxsize


pkg_name = "mood.pack"
pkg_version = "2.0.0"
pkg_desc = "mood pack module"

PKG_VERSION = ("PKG_VERSION", "\"{0}\"".format(pkg_version))


# setup
if ("sdist" not in argv):
    if (byteorder != "little"):
        raise SystemExit(f"Aborted: {pkg_name} requires a little endian CPU host")
    if (maxsize != ((2**63) - 1)):
        raise SystemExit(f"Aborted: {pkg_name} requires a 64 bits integer type")

setup(
    name=pkg_name,
    version=pkg_version,
    description=pkg_desc,
    long_description=open(abspath("README.txt"), encoding="utf-8").read(),
    long_description_content_type="text",

    url="https://github.com/lekma/mood.pack",
    download_url="https://github.com/lekma/mood.pack/releases",
    project_urls={
        "Bug Tracker": "https://github.com/lekma/mood.pack/issues"
    },
    author="Malek Hadj-Ali",
    author_email="lekmalek@gmail.com",
    license="The Unlicense (Unlicense)",
    platforms=["POSIX"],
    keywords="pack",

    setup_requires = ["setuptools>=24.2.0"],
    python_requires="~=3.10",
    packages=find_packages(),
    namespace_packages=["mood"],
    zip_safe=False,

    ext_package="mood",
    ext_modules=[
        Extension(
            "pack",
            [
                "src/helpers/helpers.c",
                "src/_pack.c",
                "src/_instance.c",
                "src/_unpack.c",
                "src/pack.c"
            ],
            define_macros=[PKG_VERSION]
        )
    ],

    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: The Unlicense (Unlicense)",
        "Operating System :: POSIX",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: Implementation :: CPython"
    ]
)
