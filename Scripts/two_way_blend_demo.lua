-- UAL1_Standard 스켈레탈 메시 액터의 ScriptPath에 연결해 PIE에서 Two-Way Blend와 BlendSpace를 함께 시연합니다.
-- [조작]
--   W / Shift+W : Locomotion BlendSpace 속도 조절 (Idle -> Walk -> Sprint)
--   Q / E       : Two-Way Blend Alpha 감소 / 증가 (0.0 <-> 1.0)
--   1 ~ 5       : Alpha 프리셋 즉시 이동 (0.00 / 0.25 / 0.50 / 0.75 / 1.00)
--   T           : Alpha 자동 왕복(Ping-Pong) 토글
--   H           : 피격(Hit_Chest) 상태 전이 테스트
local pack = "Assets/Models/UAL1/UAL1_Standard.fbx"
local walkSpeed = 1.0
local sprintSpeed = 3.0

local sm
local blendState
local hitState
local previewSpeed = 0.0
local alpha = 0.0
local autoPingPong = false
local pingPongDir = 1.0
local useMovementVelocity = false
local hitRequested = false

local function Build()
    alpha = 0.0
    autoPingPong = false
    pingPongDir = 1.0
    hitRequested = false

    local graph = obj:CreateAnimGraph()

    -- A 입력: Locomotion BlendSpace / B 입력: Sword_Attack 를 Two-Way Blend 노드로 혼합
    local locomotionSamples = {
        { take = "Idle_Loop", x = 0.0 },
        { take = "Walk_Loop", x = walkSpeed },
        { take = "Sprint_Loop", x = sprintSpeed }
    }
    blendState = graph:AddTwoWayBlendState("TwoWayBlendDemo", pack, locomotionSamples, true, "Sword_Attack", true, alpha)
    hitState = graph:AddSequenceState("Hit", pack, "Hit_Chest", false)
    assert(blendState >= 0 and hitState >= 0)

    assert(graph:AddAnyStateTransition(hitState, function()
        return hitRequested
    end))
    assert(graph:AddTransition(hitState, blendState, function()
        return graph:IsStateFinished(hitState)
    end))
    assert(graph:Start(blendState))

    sm = graph
    previewSpeed = 0.0
    useMovementVelocity = obj:HasComponent("UMovementComponent")
    print("[Lua TwoWayBlend] Ready (Q/E: Alpha, 1-5: Presets, T: Auto PingPong, W: Move, H: Hit)")
end

function BeginPlay()
    Build()
end

function OnReload()
    Build()
end

function Tick(dt)
    if not sm or not sm:IsValid() then return end

    local speed
    if useMovementVelocity then
        local velocity = obj.Velocity
        speed = math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y)
    else
        local target = IsKeyDown("W") and (IsKeyDown("shift") and sprintSpeed or walkSpeed) or 0.0
        local step = sprintSpeed * dt
        previewSpeed = previewSpeed + math.max(-step, math.min(step, target - previewSpeed))
        speed = previewSpeed
    end
    sm:SetBlendSpaceInput(blendState, speed)

    -- 현재 PIE UI에서 값을 바꿨을 수도 있으므로 최신 Alpha를 읽어온 뒤 키 입력 반영
    alpha = sm:GetTwoWayBlendAlpha(blendState)

    if IsKeyPressed("T") then
        autoPingPong = not autoPingPong
        print("[Lua TwoWayBlend] Auto Ping-Pong: " .. tostring(autoPingPong))
    end

    if IsKeyPressed("1") then alpha = 0.00; autoPingPong = false
    elseif IsKeyPressed("2") then alpha = 0.25; autoPingPong = false
    elseif IsKeyPressed("3") then alpha = 0.50; autoPingPong = false
    elseif IsKeyPressed("4") then alpha = 0.75; autoPingPong = false
    elseif IsKeyPressed("5") then alpha = 1.00; autoPingPong = false
    end

    if IsKeyDown("Q") then
        autoPingPong = false
        alpha = math.max(0.0, alpha - 0.8 * dt)
    elseif IsKeyDown("E") then
        autoPingPong = false
        alpha = math.min(1.0, alpha + 0.8 * dt)
    elseif autoPingPong then
        alpha = alpha + pingPongDir * 0.7 * dt
        if alpha >= 1.0 then
            alpha = 1.0
            pingPongDir = -1.0
        elseif alpha <= 0.0 then
            alpha = 0.0
            pingPongDir = 1.0
        end
    end

    sm:SetTwoWayBlendAlpha(blendState, alpha)
    hitRequested = IsKeyPressed("H")
end

function EndPlay()
    sm = nil
end

function OnAnimNotify(notifyName, clipName)
    print("[Lua AnimNotify] '" .. tostring(notifyName) .. "' from clip '" .. tostring(clipName) .. "'")
end

function OnAnimNotifyBegin(notifyName, clipName, duration)
    print(string.format("[Lua AnimNotifyState] Begin '%s' from clip '%s' (Duration: %.3fs)",
        tostring(notifyName), tostring(clipName), tonumber(duration) or 0))
end

function OnAnimNotifyEnd(notifyName, clipName)
    print(string.format("[Lua AnimNotifyState] End '%s' from clip '%s'",
        tostring(notifyName), tostring(clipName)))
end
