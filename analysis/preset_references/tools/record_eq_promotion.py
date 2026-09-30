"""Record explicit maintainer decisions; never rewrite the original review."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
names=['review/knowledge_candidate_review.json','review/knowledge_candidate_review.md',
       'output/preset_reference_statistics.json','output/preset_reference_inventory.json']
inputs={n:(root/n).read_bytes() for n in names}
review=json.loads(inputs[names[0]])
stats=json.loads(inputs[names[2]])
inventory=json.loads(inputs[names[3]])
assert len(review['candidates'])==18 and len(inventory['references'])==58
records=[]
for c in review['candidates']:
    promote=c['candidateId'] in ['KC-15','KC-18']
    records.append({'candidateId':c['candidateId'],
        'maintainerDecision':'PROMOTE' if promote else 'KEEP_AS_REFERENCE',
        'promotionStatus':'PromotedByMaintainer' if promote else 'NotPromoted',
        'runtimeEntryId':{'KC-15':'dataset_eq_low_cut_110','KC-18':'dataset_eq_high_cut_8k'}.get(c['candidateId']),
        'approvedUse':{'KC-15':'General Tone Guidance','KC-18':'Contextual Tone Guidance'}.get(c['candidateId']),
        'claim':c['claim'],'evidenceIds':[p['id'] for p in c['evidence']],
        'evidenceCount':c['patchCount'],'denominator':c['denominator'],
        'creatorCount':c['creatorCount'],'confidence':c['confidence'],
        'limitations':c['limitations'],'notUniversal':True})
assert sum(c['maintainerDecision']=='PROMOTE' for c in records)==2
assert all(c['maintainerDecision']=='UNDECIDED' for c in review['candidates'])
result={'schemaVersion':1,'decisionAuthority':'Explicit maintainer approval in SELECTIVE KNOWLEDGE PROMOTION — EQ GUIDANCE ONLY',
        'baseHead':'6e1fa2dbb55f4881e7a92055afd923e959b3be0e',
        'sourceHashes':{n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},
        'decisions':records,'datasetObservation':{'parameter':'COMP ON',
            'count':stats['dataset']['effects']['comp']['enabledCount'],'denominator':58,'promotionStatus':'NotPromoted'},
        'runtimePolicy':'Only KC-15 and KC-18 are guidance. KC-17 is alternative reference data, not a formal rule. Original review remains UNDECIDED as historical review artifact.'}
assert inputs=={n:(root/n).read_bytes() for n in names}
(root/'review/knowledge_promotion_decisions.json').write_text(json.dumps(result,ensure_ascii=False,indent=2,sort_keys=True)+'\n',encoding='utf-8')
print('PASS decisions: 2 PROMOTE, 16 KEEP_AS_REFERENCE; COMP NotPromoted; original review and four evidence sources unchanged.')
