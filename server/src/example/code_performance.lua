-- code_performance.lua
-- xzc
-- 2016-03-06

require "global.test"
local json = require "lua_parson"
local xml = require "lua_rapidxml"

local js = [==[
{
    "name":"xzc",
    "chinese":"中文",
    "emoji":[ "\ue513","\u2600","\u2601","\u2614" ]
}
]==]

f_tm_start()
local json_tb = json.decode( js )
f_tm_stop( "json decode cost")
vd( json_tb )

--json.encode_to_file( json_tb,"json_test.json",true )

local xml_str = [==[
<DEMDataSet xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.nemsis.org http://nemsis.org/media/nemsis_v3/release-3.4.0/XSDs/NEMSIS_XSDs/DEMDataSet_v3.xsd" xmlns="http://www.nemsis.org">
    <DemographicReport timeStamp="2015-03-03T13:29:41+07:00">
        <dAgency>
            <dAgency.01>5</dAgency.01>
            <dAgency.02>55</dAgency.02>
            <dAgency.03>z5</dAgency.03>
            <dAgency.04>49</dAgency.04>
            <dAgency.AgencyServiceGroup>
                <dAgency.05>49</dAgency.05>
                <dAgency.06>49005</dAgency.06>
                <dAgency.06>49005</dAgency.06>
                <dAgency.07>74178453211</dAgency.07>
                <dAgency.07>18743482072</dAgency.07>
                <dAgency.07>71874336709</dAgency.07>
                <dAgency.08>84327</dAgency.08>
                <dAgency.08>84327</dAgency.08></dAgency.AgencyServiceGroup>
            <dAgency.AgencyServiceGroup>
                <dAgency.05>49</dAgency.05>
                <dAgency.06>49005</dAgency.06>
                <dAgency.06>49005</dAgency.06>
                <dAgency.06>49005</dAgency.06>
                <dAgency.07>32231299726</dAgency.07>
                <dAgency.07>29697864325</dAgency.07>
                <dAgency.07>17312879111</dAgency.07>
                <dAgency.08>84327</dAgency.08>
                <dAgency.08>84327</dAgency.08>
                <dAgency.08>84327</dAgency.08></dAgency.AgencyServiceGroup>
            <dAgency.AgencyServiceGroup>
                <dAgency.05>49</dAgency.05>
                <dAgency.06>49005</dAgency.06>
                <dAgency.06>49005</dAgency.06>
                <dAgency.07>15144482342</dAgency.07>
                <dAgency.07>17515164872</dAgency.07>
                <dAgency.08>84327</dAgency.08>
                <dAgency.08>84327</dAgency.08></dAgency.AgencyServiceGroup>
            <dAgency.09>9920019</dAgency.09>
            <dAgency.10>9920019</dAgency.10>
            <dAgency.10>9920017</dAgency.10>
            <dAgency.10>9920015</dAgency.10>
            <dAgency.11>9917031</dAgency.11>
            <dAgency.12>1016005</dAgency.12>
            <dAgency.13>9912009</dAgency.13>
            <dAgency.14>1018005</dAgency.14>
            <dAgency.AgencyYearGroup>
                <dAgency.15>1904</dAgency.15>
                <dAgency.16>467170</dAgency.16>
                <dAgency.17>817994</dAgency.17>
                <dAgency.18>2974500</dAgency.18>
                <dAgency.19>113716</dAgency.19>
                <dAgency.20>1153867</dAgency.20>
                <dAgency.21>1511304</dAgency.21>
                <dAgency.22>3540798</dAgency.22></dAgency.AgencyYearGroup>
            <dAgency.AgencyYearGroup>
                <dAgency.15>2046</dAgency.15>
                <dAgency.16>2521236</dAgency.16>
                <dAgency.17>3178460</dAgency.17>
                <dAgency.18>2334931</dAgency.18>
                <dAgency.19>2340703</dAgency.19>
                <dAgency.20>2568932</dAgency.20>
                <dAgency.21>457065</dAgency.21>
                <dAgency.22>1184571</dAgency.22></dAgency.AgencyYearGroup>
            <dAgency.AgencyYearGroup>
                <dAgency.15>1988</dAgency.15>
                <dAgency.16>3443710</dAgency.16>
                <dAgency.17>3639583</dAgency.17>
                <dAgency.18>1935281</dAgency.18>
                <dAgency.19>1987971</dAgency.19>
                <dAgency.20>3145911</dAgency.20>
                <dAgency.21>2176416</dAgency.21>
                <dAgency.22>1875697</dAgency.22></dAgency.AgencyYearGroup>
            <dAgency.23>1027017</dAgency.23>
            <dAgency.24>9923003</dAgency.24>
            <dAgency.25>lMGRHKqXUK</dAgency.25>
            <dAgency.25>zuxqutfNIV</dAgency.25>
            <dAgency.25>DkJiD8GzTb</dAgency.25>
            <dAgency.26>6</dAgency.26>
            <dAgency.26>x</dAgency.26>
            <dAgency.26>N</dAgency.26></dAgency>
        <dContact>
            <dContact.ContactInfoGroup>
                <dContact.01>1101017</dContact.01>
                <dContact.02>Mitchell</dContact.02>
                <dContact.03>William</dContact.03>
                <dContact.04>Adair</dContact.04>
                <dContact.05 StreetAddress2="6">334-4920 Elementum Rd.</dContact.05>
                <dContact.06>1445667</dContact.06>
                <dContact.07>49</dContact.07>
                <dContact.08>84404</dContact.08>
                <dContact.09>US</dContact.09>
                <dContact.10 PhoneNumberType="9913009">348-308-1582</dContact.10>
                <dContact.10 PhoneNumberType="9913009">728-758-4288</dContact.10>
                <dContact.11 EmailAddressType="9904003">q7y6fA9N@5Zj3.com</dContact.11>
                <dContact.11 EmailAddressType="9904003">cWYUQD4Y@W50M.com</dContact.11>
                <dContact.11 EmailAddressType="9904003">GmDu3BCN@vDCC.com</dContact.11>
                <dContact.12>4FE</dContact.12>
                <dContact.EMSMedicalDirectorGroup>
                    <dContact.13>1113003</dContact.13>
                    <dContact.14>1114043</dContact.14>
                    <dContact.14>1114041</dContact.14>
                    <dContact.15>1115003</dContact.15>
                    <dContact.16>9923003</dContact.16></dContact.EMSMedicalDirectorGroup></dContact.ContactInfoGroup>
            <dContact.ContactInfoGroup>
                <dContact.01>1101017</dContact.01>
                <dContact.02>Gutierrez</dContact.02>
                <dContact.03>Heather</dContact.03>
                <dContact.04>Alba</dContact.04>
                <dContact.05 StreetAddress2="e">549-345 Sit Rd.</dContact.05>
                <dContact.06>1434127</dContact.06>
                <dContact.07>49</dContact.07>
                <dContact.08>84404</dContact.08>
                <dContact.09>US</dContact.09>
                <dContact.10 PhoneNumberType="9913009">441-483-1464</dContact.10>
                <dContact.10 PhoneNumberType="9913009">438-844-8188</dContact.10>
                <dContact.11 EmailAddressType="9904003">ZgsqBkSm@ETX2.com</dContact.11>
                <dContact.11 EmailAddressType="9904003">7T7j0wwW@Q53R.com</dContact.11>
                <dContact.11 EmailAddressType="9904003">ewdlldFX@M1mP.com</dContact.11>
                <dContact.12>R5V</dContact.12>
                <dContact.EMSMedicalDirectorGroup>
                    <dContact.13>1113003</dContact.13>
                    <dContact.14>1114043</dContact.14>
                    <dContact.14>1114041</dContact.14>
                    <dContact.15>1115003</dContact.15>
                    <dContact.16>9923003</dContact.16></dContact.EMSMedicalDirectorGroup></dContact.ContactInfoGroup></dContact>
        <dConfiguration>
            <dConfiguration.ConfigurationGroup>
                <dConfiguration.01>49</dConfiguration.01>
                <dConfiguration.02>9911033</dConfiguration.02>
                <dConfiguration.02>9911031</dConfiguration.02>
                <dConfiguration.02>9911029</dConfiguration.02>
                <dConfiguration.03>445392009</dConfiguration.03>
                <dConfiguration.03>445141005</dConfiguration.03>
                <dConfiguration.04>3648</dConfiguration.04>
                <dConfiguration.04>36676</dConfiguration.04>
                <dConfiguration.05>9914225</dConfiguration.05>
                <dConfiguration.05>9914223</dConfiguration.05>
                <dConfiguration.05>9914221</dConfiguration.05>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>422744007</dConfiguration.07>
                    <dConfiguration.07>425519007</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>428803005</dConfiguration.07>
                    <dConfiguration.07>278297009</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>72236</dConfiguration.09>
                    <dConfiguration.09>7238</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>7242</dConfiguration.09>
                    <dConfiguration.09>730781</dConfiguration.09>
                    <dConfiguration.09>73137</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>7396</dConfiguration.09>
                    <dConfiguration.09>7476</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.10>9914225</dConfiguration.10>
                <dConfiguration.10>9914223</dConfiguration.10>
                <dConfiguration.10>9914221</dConfiguration.10>
                <dConfiguration.11>1211035</dConfiguration.11>
                <dConfiguration.11>1211033</dConfiguration.11>
                <dConfiguration.12>9923003</dConfiguration.12>
                <dConfiguration.13>1213005</dConfiguration.13>
                <dConfiguration.14>F2</dConfiguration.14>
                <dConfiguration.14>AG</dConfiguration.14>
                <dConfiguration.15>1215019</dConfiguration.15>
                <dConfiguration.15>1215017</dConfiguration.15>
                <dConfiguration.15>1215015</dConfiguration.15>
                <dConfiguration.16>H</dConfiguration.16>
                <dConfiguration.16>Y</dConfiguration.16>
                <dConfiguration.16>m</dConfiguration.16>
                <dConfiguration.17>0X</dConfiguration.17>
                <dConfiguration.17>rw</dConfiguration.17></dConfiguration.ConfigurationGroup>
            <dConfiguration.ConfigurationGroup>
                <dConfiguration.01>49</dConfiguration.01>
                <dConfiguration.02>9911033</dConfiguration.02>
                <dConfiguration.02>9911031</dConfiguration.02>
                <dConfiguration.03>182692007</dConfiguration.03>
                <dConfiguration.03>7443007</dConfiguration.03>
                <dConfiguration.03>673005</dConfiguration.03>
                <dConfiguration.04>377965</dConfiguration.04>
                <dConfiguration.04>3827</dConfiguration.04>
                <dConfiguration.05>9914225</dConfiguration.05>
                <dConfiguration.05>9914223</dConfiguration.05>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>89666000</dConfiguration.07>
                    <dConfiguration.07>250980009</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>23852006</dConfiguration.07>
                    <dConfiguration.07>62972009</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>7486</dConfiguration.09>
                    <dConfiguration.09>7512</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>75635</dConfiguration.09>
                    <dConfiguration.09>76895</dConfiguration.09>
                    <dConfiguration.09>10154</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>10368</dConfiguration.09>
                    <dConfiguration.09>10391</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.10>9914225</dConfiguration.10>
                <dConfiguration.10>9914223</dConfiguration.10>
                <dConfiguration.10>9914221</dConfiguration.10>
                <dConfiguration.11>1211035</dConfiguration.11>
                <dConfiguration.11>1211033</dConfiguration.11>
                <dConfiguration.12>9923003</dConfiguration.12>
                <dConfiguration.13>1213005</dConfiguration.13>
                <dConfiguration.14>p5</dConfiguration.14>
                <dConfiguration.14>9W</dConfiguration.14>
                <dConfiguration.14>6H</dConfiguration.14>
                <dConfiguration.15>1215019</dConfiguration.15>
                <dConfiguration.15>1215017</dConfiguration.15>
                <dConfiguration.15>1215015</dConfiguration.15>
                <dConfiguration.16>t</dConfiguration.16>
                <dConfiguration.16>N</dConfiguration.16>
                <dConfiguration.17>HG</dConfiguration.17>
                <dConfiguration.17>Q8</dConfiguration.17>
                <dConfiguration.17>fO</dConfiguration.17></dConfiguration.ConfigurationGroup>
            <dConfiguration.ConfigurationGroup>
                <dConfiguration.01>49</dConfiguration.01>
                <dConfiguration.02>9911033</dConfiguration.02>
                <dConfiguration.02>9911031</dConfiguration.02>
                <dConfiguration.02>9911029</dConfiguration.02>
                <dConfiguration.03>19861002</dConfiguration.03>
                <dConfiguration.03>232675003</dConfiguration.03>
                <dConfiguration.04>4177</dConfiguration.04>
                <dConfiguration.04>4249</dConfiguration.04>
                <dConfiguration.04>4256</dConfiguration.04>
                <dConfiguration.05>9914225</dConfiguration.05>
                <dConfiguration.05>9914223</dConfiguration.05>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>268400002</dConfiguration.07>
                    <dConfiguration.07>243140006</dConfiguration.07>
                    <dConfiguration.07>243142003</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>44806002</dConfiguration.07>
                    <dConfiguration.07>2267008</dConfiguration.07>
                    <dConfiguration.07>425447009</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.ProcedureGroup>
                    <dConfiguration.06>9917031</dConfiguration.06>
                    <dConfiguration.07>429705000</dConfiguration.07>
                    <dConfiguration.07>47545007</dConfiguration.07></dConfiguration.ProcedureGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>10454</dConfiguration.09>
                    <dConfiguration.09>107129</dConfiguration.09>
                    <dConfiguration.09>11149</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.MedicationGroup>
                    <dConfiguration.08>9917031</dConfiguration.08>
                    <dConfiguration.09>11170</dConfiguration.09>
                    <dConfiguration.09>115698</dConfiguration.09>
                    <dConfiguration.09>1191</dConfiguration.09></dConfiguration.MedicationGroup>
                <dConfiguration.10>9914225</dConfiguration.10>
                <dConfiguration.10>9914223</dConfiguration.10>
                <dConfiguration.10>9914221</dConfiguration.10>
                <dConfiguration.11>1211035</dConfiguration.11>
                <dConfiguration.11>1211033</dConfiguration.11>
                <dConfiguration.12>9923003</dConfiguration.12>
                <dConfiguration.13>1213005</dConfiguration.13>
                <dConfiguration.14>A0</dConfiguration.14>
                <dConfiguration.14>6E</dConfiguration.14>
                <dConfiguration.14>c6</dConfiguration.14>
                <dConfiguration.15>1215019</dConfiguration.15>
                <dConfiguration.15>1215017</dConfiguration.15>
                <dConfiguration.16>N</dConfiguration.16>
                <dConfiguration.16>4</dConfiguration.16>
                <dConfiguration.16>t</dConfiguration.16>
                <dConfiguration.17>Wk</dConfiguration.17>
                <dConfiguration.17>l5</dConfiguration.17>
                <dConfiguration.17>xR</dConfiguration.17></dConfiguration.ConfigurationGroup></dConfiguration>
        <dLocation>
            <dLocation.LocationGroup>
                <dLocation.01>1301007</dLocation.01>
                <dLocation.02>rE</dLocation.02>
                <dLocation.03>Q</dLocation.03>
                <dLocation.04>41.300877,-111.946406</dLocation.04>
                <dLocation.05>16SLM02760232</dLocation.05>
                <dLocation.06 StreetAddress2="A">4326 Ipsum St.</dLocation.06>
                <dLocation.07>1444049</dLocation.07>
                <dLocation.08>49</dLocation.08>
                <dLocation.09>84412</dLocation.09>
                <dLocation.10>49057</dLocation.10>
                <dLocation.11>US</dLocation.11>
                <dLocation.12 PhoneNumberType="9913009">804-829-2579</dLocation.12>
                <dLocation.12 PhoneNumberType="9913009">264-423-3511</dLocation.12>
                <dLocation.12 PhoneNumberType="9913009">571-693-8761</dLocation.12></dLocation.LocationGroup>
            <dLocation.LocationGroup>
                <dLocation.01>1301007</dLocation.01>
                <dLocation.02>7o</dLocation.02>
                <dLocation.03>W</dLocation.03>
                <dLocation.04>37.080345,-113.575391</dLocation.04>
                <dLocation.05>10SGE97535618</dLocation.05>
                <dLocation.06 StreetAddress2="x">863-290 Rutrum Street</dLocation.06>
                <dLocation.07>1437504</dLocation.07>
                <dLocation.08>49</dLocation.08>
                <dLocation.09>84790</dLocation.09>
                <dLocation.10>49053</dLocation.10>
                <dLocation.11>US</dLocation.11>
                <dLocation.12 PhoneNumberType="9913009">573-626-3445</dLocation.12>
                <dLocation.12 PhoneNumberType="9913009">325-406-3323</dLocation.12></dLocation.LocationGroup></dLocation>
        <dVehicle>
            <dVehicle.VehicleGroup>
                <dVehicle.01>g</dVehicle.01>
                <dVehicle.02>ozo2opX9bqXzSpzww</dVehicle.02>
                <dVehicle.03>h</dVehicle.03>
                <dVehicle.04>1404025</dVehicle.04>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>11</dVehicle.06>
                    <dVehicle.07>11</dVehicle.07>
                    <dVehicle.08>5</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>10</dVehicle.06>
                    <dVehicle.07>5</dVehicle.07>
                    <dVehicle.08>3</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.09>3577743</dVehicle.09>
                <dVehicle.10>2035</dVehicle.10>
                <dVehicle.YearGroup>
                    <dVehicle.11>1973</dVehicle.11>
                    <dVehicle.12>7814</dVehicle.12>
                    <dVehicle.13 DistanceUnit="9929003">347.0</dVehicle.13></dVehicle.YearGroup>
                <dVehicle.YearGroup>
                    <dVehicle.11>1959</dVehicle.11>
                    <dVehicle.12>2393</dVehicle.12>
                    <dVehicle.13 DistanceUnit="9929003">710.0</dVehicle.13></dVehicle.YearGroup></dVehicle.VehicleGroup>
            <dVehicle.VehicleGroup>
                <dVehicle.01>7</dVehicle.01>
                <dVehicle.02>PnyuBySpGesmG905n</dVehicle.02>
                <dVehicle.03>E</dVehicle.03>
                <dVehicle.04>1404025</dVehicle.04>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>1</dVehicle.06>
                    <dVehicle.07>3</dVehicle.07>
                    <dVehicle.08>10</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>3</dVehicle.06>
                    <dVehicle.07>7</dVehicle.07>
                    <dVehicle.08>4</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>1</dVehicle.06>
                    <dVehicle.07>10</dVehicle.07>
                    <dVehicle.08>9</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.09>7977085</dVehicle.09>
                <dVehicle.10>1996</dVehicle.10>
                <dVehicle.YearGroup>
                    <dVehicle.11>1977</dVehicle.11>
                    <dVehicle.12>5218</dVehicle.12>
                    <dVehicle.13 DistanceUnit="9929003">675.0</dVehicle.13></dVehicle.YearGroup>
                <dVehicle.YearGroup>
                    <dVehicle.11>1905</dVehicle.11>
                    <dVehicle.12>3946</dVehicle.12>
                    <dVehicle.13 DistanceUnit="9929003">599.0</dVehicle.13></dVehicle.YearGroup></dVehicle.VehicleGroup>
            <dVehicle.VehicleGroup>
                <dVehicle.01>o</dVehicle.01>
                <dVehicle.02>GKjxWnWUIOhmCBxLz</dVehicle.02>
                <dVehicle.03>a</dVehicle.03>
                <dVehicle.04>1404025</dVehicle.04>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>6</dVehicle.06>
                    <dVehicle.07>9</dVehicle.07>
                    <dVehicle.08>6</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.VehicleCertificationLevelsGroup>
                    <dVehicle.05>9917031</dVehicle.05>
                    <dVehicle.06>2</dVehicle.06>
                    <dVehicle.07>10</dVehicle.07>
                    <dVehicle.08>2</dVehicle.08></dVehicle.VehicleCertificationLevelsGroup>
                <dVehicle.09>1732884</dVehicle.09>
                <dVehicle.10>1937</dVehicle.10>
                <dVehicle.YearGroup>
                    <dVehicle.11>2030</dVehicle.11>
                    <dVehicle.12>5661</dVehicle.12>
                    <dVehicle.13 DistanceUnit="9929003">697.0</dVehicle.13></dVehicle.YearGroup>
                <dVehicle.YearGroup>
                    <dVehicle.11>1957</dVehicle.11>
                    <dVehicle.12>9344</dVehicle.12>
                    <dVehicle.13 DistanceUnit="9929003">282.0</dVehicle.13></dVehicle.YearGroup></dVehicle.VehicleGroup></dVehicle>
        <dPersonnel>
            <dPersonnel.PersonnelGroup>
                <dPersonnel.NameGroup>
                    <dPersonnel.01>Pierce</dPersonnel.01>
                    <dPersonnel.02>Mona</dPersonnel.02>
                    <dPersonnel.03>Alston</dPersonnel.03></dPersonnel.NameGroup>
                <dPersonnel.AddressGroup>
                    <dPersonnel.04 StreetAddress2="C">860 Risus. Street</dPersonnel.04>
                    <dPersonnel.05>1430684</dPersonnel.05>
                    <dPersonnel.06>49</dPersonnel.06>
                    <dPersonnel.07>84053</dPersonnel.07>
                    <dPersonnel.08>US</dPersonnel.08></dPersonnel.AddressGroup>
                <dPersonnel.09 PhoneNumberType="9913009">776-428-5333</dPersonnel.09>
                <dPersonnel.09 PhoneNumberType="9913009">728-316-6217</dPersonnel.09>
                <dPersonnel.09 PhoneNumberType="9913009">542-408-7233</dPersonnel.09>
                <dPersonnel.10 EmailAddressType="9904003">moeVU0eC@0CUc.com</dPersonnel.10>
                <dPersonnel.10 EmailAddressType="9904003">GtZwno2W@mbDx.com</dPersonnel.10>
                <dPersonnel.11>2007-10-14</dPersonnel.11>
                <dPersonnel.12>9906005</dPersonnel.12>
                <dPersonnel.13>1513011</dPersonnel.13>
                <dPersonnel.13>1513009</dPersonnel.13>
                <dPersonnel.14>ss</dPersonnel.14>
                <dPersonnel.15>1515031</dPersonnel.15>
                <dPersonnel.16>1516071</dPersonnel.16>
                <dPersonnel.16>1516069</dPersonnel.16>
                <dPersonnel.17>1517023</dPersonnel.17>
                <dPersonnel.17>1517021</dPersonnel.17>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>1915</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>1910</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>1952</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.20>yid</dPersonnel.20>
                <dPersonnel.20>vie</dPersonnel.20>
                <dPersonnel.21>Ky</dPersonnel.21>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>lM</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>1999-03-15</dPersonnel.25>
                    <dPersonnel.26>2003-10-24</dPersonnel.26>
                    <dPersonnel.27>1957-04-07</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>vM</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>1993-05-19</dPersonnel.25>
                    <dPersonnel.26>1987-06-07</dPersonnel.26>
                    <dPersonnel.27>2001-05-13</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.28>Z4463477</dPersonnel.28>
                <dPersonnel.29>1529015</dPersonnel.29>
                <dPersonnel.30>2007-03-26</dPersonnel.30>
                <dPersonnel.31>1531007</dPersonnel.31>
                <dPersonnel.32>1996-06-08</dPersonnel.32>
                <dPersonnel.33>2007-09-27</dPersonnel.33>
                <dPersonnel.34>1534017</dPersonnel.34>
                <dPersonnel.35>1534017</dPersonnel.35>
                <dPersonnel.35>1534015</dPersonnel.35>
                <dPersonnel.35>1534013</dPersonnel.35>
                <dPersonnel.36>9</dPersonnel.36>
                <dPersonnel.37>1950-05-23</dPersonnel.37>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>2011-12-07</dPersonnel.39></dPersonnel.CertificationLevelGroup>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>1951-07-18</dPersonnel.39></dPersonnel.CertificationLevelGroup></dPersonnel.PersonnelGroup>
            <dPersonnel.PersonnelGroup>
                <dPersonnel.NameGroup>
                    <dPersonnel.01>Ingram</dPersonnel.01>
                    <dPersonnel.02>Kristen</dPersonnel.02>
                    <dPersonnel.03>Acker</dPersonnel.03></dPersonnel.NameGroup>
                <dPersonnel.AddressGroup>
                    <dPersonnel.04 StreetAddress2="v">Ap #893-9472 Et Rd.</dPersonnel.04>
                    <dPersonnel.05>1441004</dPersonnel.05>
                    <dPersonnel.06>49</dPersonnel.06>
                    <dPersonnel.07>84025</dPersonnel.07>
                    <dPersonnel.08>US</dPersonnel.08></dPersonnel.AddressGroup>
                <dPersonnel.09 PhoneNumberType="9913009">973-528-2237</dPersonnel.09>
                <dPersonnel.09 PhoneNumberType="9913009">314-373-0363</dPersonnel.09>
                <dPersonnel.10 EmailAddressType="9904003">EdQR5YCx@lDPK.com</dPersonnel.10>
                <dPersonnel.10 EmailAddressType="9904003">PzzzT9GM@F3k0.com</dPersonnel.10>
                <dPersonnel.11>2001-11-24</dPersonnel.11>
                <dPersonnel.12>9906005</dPersonnel.12>
                <dPersonnel.13>1513011</dPersonnel.13>
                <dPersonnel.13>1513009</dPersonnel.13>
                <dPersonnel.13>1513007</dPersonnel.13>
                <dPersonnel.14>bJ</dPersonnel.14>
                <dPersonnel.15>1515031</dPersonnel.15>
                <dPersonnel.16>1516071</dPersonnel.16>
                <dPersonnel.16>1516069</dPersonnel.16>
                <dPersonnel.17>1517023</dPersonnel.17>
                <dPersonnel.17>1517021</dPersonnel.17>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>2010</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>2042</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>2013</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.20>yid</dPersonnel.20>
                <dPersonnel.20>vie</dPersonnel.20>
                <dPersonnel.21>HB</dPersonnel.21>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>Rw</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>1956-07-17</dPersonnel.25>
                    <dPersonnel.26>2008-04-08</dPersonnel.26>
                    <dPersonnel.27>1974-09-14</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>wE</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>1959-03-18</dPersonnel.25>
                    <dPersonnel.26>2003-06-26</dPersonnel.26>
                    <dPersonnel.27>1986-03-16</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.28>GU876163</dPersonnel.28>
                <dPersonnel.29>1529015</dPersonnel.29>
                <dPersonnel.30>1961-04-04</dPersonnel.30>
                <dPersonnel.31>1531007</dPersonnel.31>
                <dPersonnel.32>1965-11-10</dPersonnel.32>
                <dPersonnel.33>1978-06-06</dPersonnel.33>
                <dPersonnel.34>1534017</dPersonnel.34>
                <dPersonnel.35>1534017</dPersonnel.35>
                <dPersonnel.35>1534015</dPersonnel.35>
                <dPersonnel.35>1534013</dPersonnel.35>
                <dPersonnel.36>17</dPersonnel.36>
                <dPersonnel.37>2008-02-06</dPersonnel.37>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>1998-02-18</dPersonnel.39></dPersonnel.CertificationLevelGroup>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>1956-07-23</dPersonnel.39></dPersonnel.CertificationLevelGroup>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>1988-01-26</dPersonnel.39></dPersonnel.CertificationLevelGroup></dPersonnel.PersonnelGroup>
            <dPersonnel.PersonnelGroup>
                <dPersonnel.NameGroup>
                    <dPersonnel.01>Ballard</dPersonnel.01>
                    <dPersonnel.02>Lauren</dPersonnel.02>
                    <dPersonnel.03>Almeida</dPersonnel.03></dPersonnel.NameGroup>
                <dPersonnel.AddressGroup>
                    <dPersonnel.04 StreetAddress2="m">Ap #491-204 Augue Av.</dPersonnel.04>
                    <dPersonnel.05>1428944</dPersonnel.05>
                    <dPersonnel.06>49</dPersonnel.06>
                    <dPersonnel.07>84120</dPersonnel.07>
                    <dPersonnel.08>US</dPersonnel.08></dPersonnel.AddressGroup>
                <dPersonnel.09 PhoneNumberType="9913009">250-411-4294</dPersonnel.09>
                <dPersonnel.09 PhoneNumberType="9913009">821-531-2623</dPersonnel.09>
                <dPersonnel.10 EmailAddressType="9904003">ip07GACG@3Ojy.com</dPersonnel.10>
                <dPersonnel.10 EmailAddressType="9904003">pwUV7u4A@sQU6.com</dPersonnel.10>
                <dPersonnel.10 EmailAddressType="9904003">7U2SKPHy@Q6vx.com</dPersonnel.10>
                <dPersonnel.11>1986-06-11</dPersonnel.11>
                <dPersonnel.12>9906005</dPersonnel.12>
                <dPersonnel.13>1513011</dPersonnel.13>
                <dPersonnel.13>1513009</dPersonnel.13>
                <dPersonnel.14>u9</dPersonnel.14>
                <dPersonnel.15>1515031</dPersonnel.15>
                <dPersonnel.16>1516071</dPersonnel.16>
                <dPersonnel.16>1516069</dPersonnel.16>
                <dPersonnel.16>1516067</dPersonnel.16>
                <dPersonnel.17>1517023</dPersonnel.17>
                <dPersonnel.17>1517021</dPersonnel.17>
                <dPersonnel.17>1517019</dPersonnel.17>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>2041</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.ImmunizationsGroup>
                    <dPersonnel.18>9910051</dPersonnel.18>
                    <dPersonnel.19>1963</dPersonnel.19></dPersonnel.ImmunizationsGroup>
                <dPersonnel.20>yid</dPersonnel.20>
                <dPersonnel.20>vie</dPersonnel.20>
                <dPersonnel.21>Ff</dPersonnel.21>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>QN</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>1976-11-09</dPersonnel.25>
                    <dPersonnel.26>1965-02-02</dPersonnel.26>
                    <dPersonnel.27>1998-09-19</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>pw</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>2011-09-21</dPersonnel.25>
                    <dPersonnel.26>1996-07-21</dPersonnel.26>
                    <dPersonnel.27>1988-11-22</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.LicensureGroup>
                    <dPersonnel.22>49</dPersonnel.22>
                    <dPersonnel.23>1N</dPersonnel.23>
                    <dPersonnel.24>9925043</dPersonnel.24>
                    <dPersonnel.25>1985-04-10</dPersonnel.25>
                    <dPersonnel.26>2011-08-24</dPersonnel.26>
                    <dPersonnel.27>1960-10-01</dPersonnel.27></dPersonnel.LicensureGroup>
                <dPersonnel.28>PT662707</dPersonnel.28>
                <dPersonnel.29>1529015</dPersonnel.29>
                <dPersonnel.30>2008-05-27</dPersonnel.30>
                <dPersonnel.31>1531007</dPersonnel.31>
                <dPersonnel.32>2007-04-26</dPersonnel.32>
                <dPersonnel.33>2011-05-28</dPersonnel.33>
                <dPersonnel.34>1534017</dPersonnel.34>
                <dPersonnel.35>1534017</dPersonnel.35>
                <dPersonnel.35>1534015</dPersonnel.35>
                <dPersonnel.35>1534013</dPersonnel.35>
                <dPersonnel.36>23</dPersonnel.36>
                <dPersonnel.37>1959-11-15</dPersonnel.37>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>1995-06-29</dPersonnel.39></dPersonnel.CertificationLevelGroup>
                <dPersonnel.CertificationLevelGroup>
                    <dPersonnel.38>9925043</dPersonnel.38>
                    <dPersonnel.39>1969-08-06</dPersonnel.39></dPersonnel.CertificationLevelGroup></dPersonnel.PersonnelGroup></dPersonnel>
        <dDevice>
            <dDevice.DeviceGroup>
                <dDevice.01>C6</dDevice.01>
                <dDevice.02>Q1</dDevice.02>
                <dDevice.03>1603039</dDevice.03>
                <dDevice.03>1603037</dDevice.03>
                <dDevice.04>j3</dDevice.04>
                <dDevice.05>4q</dDevice.05>
                <dDevice.06>1958-11-20</dDevice.06></dDevice.DeviceGroup>
            <dDevice.DeviceGroup>
                <dDevice.01>SZ</dDevice.01>
                <dDevice.02>76</dDevice.02>
                <dDevice.03>1603039</dDevice.03>
                <dDevice.03>1603037</dDevice.03>
                <dDevice.04>Bf</dDevice.04>
                <dDevice.05>3g</dDevice.05>
                <dDevice.06>1989-06-13</dDevice.06></dDevice.DeviceGroup>
            <dDevice.DeviceGroup>
                <dDevice.01>mI</dDevice.01>
                <dDevice.02>nt</dDevice.02>
                <dDevice.03>1603039</dDevice.03>
                <dDevice.03>1603037</dDevice.03>
                <dDevice.03>1603035</dDevice.03>
                <dDevice.04>hx</dDevice.04>
                <dDevice.05>gs</dDevice.05>
                <dDevice.06>1996-08-23</dDevice.06></dDevice.DeviceGroup></dDevice>
        <dFacility>
            <dFacilityGroup>
                <dFacility.01>1701017</dFacility.01>
                <dFacility.FacilityGroup>
                    <dFacility.02>CV</dFacility.02>
                    <dFacility.03>3T</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.05>FzlcC2qwmi</dFacility.05>
                    <dFacility.05>63Jy896jrF</dFacility.05>
                    <dFacility.05>pNKmBRqWYv</dFacility.05>
                    <dFacility.06>Y</dFacility.06>
                    <dFacility.07 StreetAddress2="k">8429 Faucibus Road</dFacility.07>
                    <dFacility.08>1430394</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84753</dFacility.10>
                    <dFacility.11>49021</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>37.798279,-113.906685</dFacility.13>
                    <dFacility.14>17UJT14582454</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">577-724-5357</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">650-532-6430</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">816-727-5776</dFacility.15></dFacility.FacilityGroup>
                <dFacility.FacilityGroup>
                    <dFacility.02>Rt</dFacility.02>
                    <dFacility.03>gP</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.04>9908031</dFacility.04>
                    <dFacility.05>44rhWtmDEK</dFacility.05>
                    <dFacility.05>sYkrRYwHWE</dFacility.05>
                    <dFacility.05>2GnW6yhLxA</dFacility.05>
                    <dFacility.06>A</dFacility.06>
                    <dFacility.07 StreetAddress2="x">407-944 Egestas St.</dFacility.07>
                    <dFacility.08>1433054</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84772</dFacility.10>
                    <dFacility.11>49021</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>37.797218,-112.942812</dFacility.13>
                    <dFacility.14>16UWL37456414</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">756-377-8595</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">527-615-3376</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">613-355-2273</dFacility.15></dFacility.FacilityGroup>
                <dFacility.FacilityGroup>
                    <dFacility.02>Iy</dFacility.02>
                    <dFacility.03>ok</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.04>9908031</dFacility.04>
                    <dFacility.05>NDaeVEZbzE</dFacility.05>
                    <dFacility.05>dtxHzA8Bh2</dFacility.05>
                    <dFacility.05>G8AiRWIIIG</dFacility.05>
                    <dFacility.06>I</dFacility.06>
                    <dFacility.07 StreetAddress2="N">Ap #110-3625 Et, Ave</dFacility.07>
                    <dFacility.08>1445269</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84654</dFacility.10>
                    <dFacility.11>49041</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>38.951768,-111.860719</dFacility.13>
                    <dFacility.14>15SDL77062831</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">543-778-9254</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">848-336-5653</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">599-284-8815</dFacility.15></dFacility.FacilityGroup></dFacilityGroup>
            <dFacilityGroup>
                <dFacility.01>1701017</dFacility.01>
                <dFacility.FacilityGroup>
                    <dFacility.02>E9</dFacility.02>
                    <dFacility.03>EP</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.04>9908031</dFacility.04>
                    <dFacility.05>FBOvf1YIGv</dFacility.05>
                    <dFacility.05>lsjf2yB6sl</dFacility.05>
                    <dFacility.05>gQLdvNBFok</dFacility.05>
                    <dFacility.06>Z</dFacility.06>
                    <dFacility.07 StreetAddress2="F">Ap #785-1600 Fringilla, Road</dFacility.07>
                    <dFacility.08>1454997</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84171</dFacility.10>
                    <dFacility.11>49035</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>40.620584,-111.806052</dFacility.13>
                    <dFacility.14>17TXM69442052</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">576-624-4563</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">371-491-5948</dFacility.15></dFacility.FacilityGroup>
                <dFacility.FacilityGroup>
                    <dFacility.02>Ee</dFacility.02>
                    <dFacility.03>rP</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.05>kUUCiSrSWg</dFacility.05>
                    <dFacility.05>qStZX1cVzq</dFacility.05>
                    <dFacility.06>O</dFacility.06>
                    <dFacility.07 StreetAddress2="2">P.O. Box 541, 6282 Malesuada Road</dFacility.07>
                    <dFacility.08>1447016</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84331</dFacility.10>
                    <dFacility.11>49003</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>41.975569,-112.191160</dFacility.13>
                    <dFacility.14>15TGS23678674</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">201-457-2288</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">578-835-2262</dFacility.15></dFacility.FacilityGroup></dFacilityGroup>
            <dFacilityGroup>
                <dFacility.01>1701017</dFacility.01>
                <dFacility.FacilityGroup>
                    <dFacility.02>8R</dFacility.02>
                    <dFacility.03>vF</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.04>9908031</dFacility.04>
                    <dFacility.05>q09IPfNCid</dFacility.05>
                    <dFacility.05>Wx6m9htdZO</dFacility.05>
                    <dFacility.06>k</dFacility.06>
                    <dFacility.07 StreetAddress2="D">Ap #384-7815 Ipsum. Ave</dFacility.07>
                    <dFacility.08>1433885</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84079</dFacility.10>
                    <dFacility.11>49047</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>40.451292,-109.536992</dFacility.13>
                    <dFacility.14>19SNK17841173</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">878-618-6974</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">289-538-5492</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">654-460-1786</dFacility.15></dFacility.FacilityGroup>
                <dFacility.FacilityGroup>
                    <dFacility.02>HI</dFacility.02>
                    <dFacility.03>qO</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.04>9908031</dFacility.04>
                    <dFacility.05>ojWdg6fStW</dFacility.05>
                    <dFacility.05>1bypoU6FTv</dFacility.05>
                    <dFacility.05>mcOQbqqJ9l</dFacility.05>
                    <dFacility.06>o</dFacility.06>
                    <dFacility.07 StreetAddress2="t">434 Et Road</dFacility.07>
                    <dFacility.08>1428726</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84635</dFacility.10>
                    <dFacility.11>49027</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>39.364440,-112.719300</dFacility.13>
                    <dFacility.14>14TFB45887325</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">854-336-2535</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">971-312-4243</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">750-938-4245</dFacility.15></dFacility.FacilityGroup>
                <dFacility.FacilityGroup>
                    <dFacility.02>Oc</dFacility.02>
                    <dFacility.03>Pb</dFacility.03>
                    <dFacility.04>9908035</dFacility.04>
                    <dFacility.04>9908033</dFacility.04>
                    <dFacility.04>9908031</dFacility.04>
                    <dFacility.05>OJdUELfexf</dFacility.05>
                    <dFacility.05>N7Oh8JlAG3</dFacility.05>
                    <dFacility.06>D</dFacility.06>
                    <dFacility.07 StreetAddress2="K">138-4403 Eu St.</dFacility.07>
                    <dFacility.08>1437516</dFacility.08>
                    <dFacility.09>49</dFacility.09>
                    <dFacility.10>84034</dFacility.10>
                    <dFacility.11>49045</dFacility.11>
                    <dFacility.12>US</dFacility.12>
                    <dFacility.13>40.037817,-113.985828</dFacility.13>
                    <dFacility.14>15RNU51607541</dFacility.14>
                    <dFacility.15 PhoneNumberType="9913009">635-228-6468</dFacility.15>
                    <dFacility.15 PhoneNumberType="9913009">788-732-4568</dFacility.15></dFacility.FacilityGroup></dFacilityGroup></dFacility></DemographicReport></DEMDataSet>
]==]

-- f_tm_start()
-- local xml_tb = xml.decode_from_file( "arena.xml" )
-- f_tm_stop( "xml decode cost")
-- vd( xml_tb )

t_describe("string extend library", function()
    t_it("string.split", function()
        local list = string.split("a,b,c", ",")
        t_equal(list, {"a", "b", "c"})
    end)
end)
