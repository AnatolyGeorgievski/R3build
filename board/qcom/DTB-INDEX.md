# board/qcom — dts/dtb по слотам umbar

<tags>qcom, dtb, dts, umbar, cdba</tags>

Каждая плата: `board/qcom/<BOARD_ID>/` — `.dts` + `.dtb` + `SOURCE.txt`.

| BOARD_ID | dts | dtb | status |
|---|---|---|---|
| db410c-1 | apq8016-sbc.dts | apq8016-sbc.dtb | saved |
| db8074-1 | — | — | missing |
| db820c-1 | apq8096-db820c.dts | apq8096-db820c.dtb | saved |
| ifc6410-1 | — | — | missing |
| ifc6560-1 | sda660-inforce-ifc6560.dts | sda660-inforce-ifc6560.dtb | saved |
| openq835-1 | msm8998-mtp.dts | msm8998-mtp.dtb | saved |
| rb1-1 | qrb2210-rb1.dts | qrb2210-rb1.dtb | saved |
| rb2-1 | qrb4210-rb2.dts | qrb4210-rb2.dtb | saved |
| rb3-1 | sdm845-db845c.dts | sdm845-db845c.dtb | saved |
| rb3gen2-1 | qcs6490-rb3gen2.dts | qcs6490-rb3gen2.dtb | saved |
| rb5-1 | qrb5165-rb5.dts | qrb5165-rb5.dtb | saved |
| rb5gen2-1 | qcs8550-rb5gen2.dts | qcs8550-rb5gen2.dtb | saved |
| sdm845-hdk-1 | sdm845-mtp.dts | sdm845-mtp.dtb | saved |
| sm8150-hdk-1 | sm8150-hdk.dts | sm8150-hdk.dtb | saved |
| sm8350-hdk-1 | sm8350-hdk.dts | sm8350-hdk.dtb | saved |
| sm8450-hdk-1 | sm8450-hdk.dts | sm8450-hdk.dtb | saved |

```bash
./scripts/save-board-dtb.sh [/path/to/arm64/boot/dts/qcom]
```
