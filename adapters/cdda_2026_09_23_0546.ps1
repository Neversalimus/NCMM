param([ValidateSet('Describe','BeforePayload','AfterPayload')][string]$Mode='Describe',[string]$ContextJson='')
$ErrorActionPreference='Stop'
$basePath=Join-Path $PSScriptRoot 'base\cdda_2026_series.ps1'
if(-not(Test-Path $basePath -PathType Leaf)){throw 'Inherited base adapter missing.'}
$base=& $basePath -Mode Describe -ContextJson $ContextJson
switch($Mode){
'Describe'{
    [pscustomobject]@{
        schema=2
        id='cdda-2026-09-23-0546-e262adb2'
        inherits=[string]$base.id
        source_family=[string]$base.source_family
        commit='e262adb299a7613b4aedc5f12c08fe0413c56a84'
        tag='cdda-experimental-2026-09-23-0546'
        folder='cdda_experimental_2026_09_23_0546'
        cache_key='cdda_0546'
        vcpkg_commit='f6672d8e480ccdecddfad3fd1b838ba369ffe6cd'
        payload=[string]$base.payload
        contract_registry=[string]$base.contract_registry
        transform_generation=[string]$base.transform_generation
        support='exact'
        structural_reuse=[bool]$base.structural_reuse
        reference_blobs=[ordered]@{
                'src/options.h' = 'a101f0a9ae381a1bd14984641420d2a67e17128a'
                'src/options.cpp' = '703c85953424019b6ecda0eb26b179f20fe0eaaa'
                'src/sdltiles.cpp' = 'c5ac1a537d580bd1930b2a41a9265b8f7a1f6406'
                'src/main_menu.cpp' = 'ea6f117cdc1a9e9785786ebf6585b39aa6803fc4'
                'src/do_turn.cpp' = '07f924d00f7f3b410bdf881461c472ce3e82515a'
                'src/input.h' = '47fcd0d9a6eaac223fefc0d938551ae4e7d76c1e'
                'src/input.cpp' = '54b91645b012b08fd71b1d558620054cce889d1b'
                'src/handle_action.cpp' = '8a3ebf77fd6a631677a99ddc2d6f9880fcedc9b5'
                'src/character.cpp' = '838812b4540beded00058a6d2b2782f5c586cc21'
                'src/character_health.cpp' = '1075811c1790fb1e241844e67aefc10d62252c51'
                'src/melee.cpp' = '8f2a3e570e18648aed83466323b9f2514762a3db'
                'src/ranged.cpp' = '54426a5dafe395306dc68fa73007f52e6ae2f645'
                'src/character_knowledge.cpp' = 'c9c09c7d8345c3217c743fe22a3965b3d23f941a'
                'src/crafting.cpp' = 'e08768cbc527b809078e2cbdce6d0dd0e7ff176f'
                'src/creature.cpp' = '0ab8460d624ba27ee1afdd241ed33fea800b37f0'
                'src/monster.cpp' = 'e6d6fff55d2c647adfd063e8bd009173307b39de'
                'src/npc.cpp' = 'eef674de308711ed7079f59c3f08dab777ef7f8b'
                'src/activity_actor.cpp' = '77807e28c54316db8f2b1890ffb81b2f24d18c6e'
                'src/trap.cpp' = '8857008a59b421d10ad40cf572c6b034bb8630f9'
                'src/debug.cpp' = '4e3c3a5c9c29c7c1748b46dc5d21fe196b2d5a50'
                'src/magic.cpp' = 'acd1ca60046ec8ad3a8d483497031ddd9eb8feaf'
                'src/overmap.cpp' = '21c6fbe123e412dce90180e3e800afec48a778b3'
                'src/overmap.h' = '97f30a06129cb137cece91bebc002c2082dedcd0'
                'src/overmap_city.cpp' = 'f0bc5f7f6c1aa89b3f06ee6f46b70546919d9429'
                'src/overmap_highway.cpp' = 'ab1fded96396fb83897a6327ccfe997d7dc5f5cd'
                'src/overmap_water.cpp' = 'c27fb0f6c03c398d4356fc5f4067f89e41e001b4'
                'src/recipe_dictionary.cpp' = '43516ee9f4682d9f5deb0281f389a51e7c956ad6'
        }
    }
}
'BeforePayload'{& $basePath -Mode BeforePayload -ContextJson $ContextJson;return}
'AfterPayload'{& $basePath -Mode AfterPayload -ContextJson $ContextJson;return}
}
