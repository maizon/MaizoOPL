import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from maizo_release_gate import assess,WORKFLOWS,VARIANTS,REQUIRED
# Synthetic fixtures only; these are not physical console reports.
sha="a"*40
runs=[dict(head_sha=sha,name=n,id=i,status="completed",conclusion="success")
      for i,n in enumerate(WORKFLOWS)]
arts=[dict(name="MAIZO-IGR-EXPERIMENTAL-ELF-"+v,expired=False,
           workflow_run={"head_sha":sha}) for v in VARIANTS]
rows=[dict(id=k,result="pass",console="synthetic fixture",evidence="synthetic fixture")
      for ids in REQUIRED.values() for k in ids]
report=dict(commit=sha,tests=rows)
assert not assess(sha,runs,arts,{})["release_ready"]
assert assess(sha,runs,arts,report)["release_ready"]
assert not assess(sha,runs,arts[:2],report)["release_ready"]
assert not assess(sha,runs,arts,dict(commit="b"*40,tests=rows))["release_ready"]
runs[0]["conclusion"]="failure"
assert not assess(sha,runs,arts,report)["release_ready"]
runs[0]["conclusion"]="success"
rows.append(dict(id="mk-armageddon-midfight",result="fail",console="fixture",evidence="fixture"))
assert not assess(sha,runs,arts,report)["release_ready"]
print("Release gate synthetic fixture tests passed")
