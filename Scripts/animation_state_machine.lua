-- 상태 판단만 확인하는 예제. 실제 애니메이션 플레이어 연결은 다음 단계다.
-- 액터의 ScriptPath에 연결 후 PIE: A 공격, H 피격, R 재피격 재시작 토글.
-- 머신 생성은 BeginPlay / OnReload에서 수행한다.
local sm
local attackRequested = false
local hitRequested = false
local restartHit = false

local function Build()
    attackRequested = false
    hitRequested = false
    sm = CreateAnimStateMachine()

    local locomotion = sm:AddState("Locomotion")
    local attack = sm:AddState("Attack")
    local hit = sm:AddState("Hit")
    assert(locomotion >= 0 and attack >= 0 and hit >= 0)

    assert(sm:AddTransition(locomotion, attack, function()
        return attackRequested
    end))
    assert(sm:AddTransition(attack, locomotion, function()
        return sm:GetStateElapsedTime() >= 1.0
    end))
    assert(sm:AddAnyStateTransition(hit, function()
        return hitRequested
    end))
    assert(sm:AddTransition(hit, locomotion, function()
        return sm:GetStateElapsedTime() >= 1.0
    end))
    assert(sm:SetStateRestartOnReentry(hit, restartHit))
    assert(sm:Start(locomotion))
    print("[Lua Animation] " .. sm:GetCurrentStateName())

    return hit
end

local hitState

function BeginPlay()
    hitState = Build()
end

function OnReload()
    hitState = Build()
end

function Tick(dt)
    if not sm or not sm:IsValid() then return end

    attackRequested = IsKeyPressed("A")
    hitRequested = IsKeyPressed("H")
    if IsKeyPressed("R") then
        restartHit = not restartHit
        assert(sm:SetStateRestartOnReentry(hitState, restartHit))
        print("[Lua Animation] Restart Hit: " .. tostring(restartHit))
    end

    if sm:Update(dt) then
        print("[Lua Animation] " .. sm:GetCurrentStateName())
    end
    attackRequested = false
    hitRequested = false
end

function EndPlay()
    sm = nil
end
