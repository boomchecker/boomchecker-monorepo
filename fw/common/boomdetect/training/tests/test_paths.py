"""The package finds the C tree it mirrors."""

from __future__ import annotations

from boomdetect_train import paths


def test_c_tree_is_where_expected():
    assert (paths.BOOMDETECT_DIR / "CMakeLists.txt").is_file()
    assert paths.MFCC_TABLES_H.is_file()
    assert (paths.MODELS_DIR / "model_mlp_v6.c").is_file()
    assert paths.SELFTEST_HOST_FIXTURE.is_file()
    assert (paths.REPO_ROOT / "fw" / "common" / "boomdetect").samefile(paths.BOOMDETECT_DIR)


def test_data_root_is_outside_the_repository():
    root = paths.data_root().resolve()
    assert paths.REPO_ROOT.resolve() not in root.parents
