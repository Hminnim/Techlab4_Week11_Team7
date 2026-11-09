-- UAL1_Standard 스켈레탈 메시 액터의 ScriptPath에 연결한 뒤 PIE에서 사용한다.
local pack = "Assets/Models/UAL1/UAL1_Standard.fbx"
local ready = false

local function Setup()
    ready = obj:ConfigureCharacterAnimations(
        pack, pack, pack,
        "Armature|Idle_Loop", "Armature|Sword_Attack", "Armature|Hit_Chest"
    )
    obj:SetRestartHitOnReentry(script_state.restartHit == true)
end

function BeginPlay()
    Setup()
end

function OnReload()
    Setup()
end

function Tick(dt)
    if not ready then return end

    if IsKeyPressed("A") then
        obj:RequestAnimAttack()
    end
    if IsKeyPressed("H") then
        obj:RequestAnimHit()
    end
    if IsKeyPressed("R") then
        script_state.restartHit = not script_state.restartHit
        obj:SetRestartHitOnReentry(script_state.restartHit)
    end
end
